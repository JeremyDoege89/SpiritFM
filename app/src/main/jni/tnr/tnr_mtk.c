
    // Spirit Tuner Plugin for the MediaTek "/dev/fm" API (connsys FM: MT6631/MT6635 and relatives on MT68xx SoCs).

    // Ioctls and structures follow MediaTek's FM HAL (fm.h / common.cpp in AOSP-derived FMRadio, Apache 2.0).
    // Frequencies: Spirit uses kHz (88500); the driver takes 100 kHz units for tune (885) and 10 kHz units for soft mute tune (8850).

  #define LOGTAG "sftnrmtk"

  #include <stdio.h>
  #include <errno.h>
  #include <sys/stat.h>
  #include <fcntl.h>
  #include <pthread.h>
  #include <stdint.h>

  #include <sys/ioctl.h>

  #include "tnr_tnr.c"

    // Driver API:

  #define FM_IOC_MAGIC        0xf5

  struct fm_tune_parm {
    uint8_t  err;
    uint8_t  band;
    uint8_t  space;
    uint8_t  hilo;
    uint16_t freq;
  };

  struct fm_softmute_tune_t {
    int32_t  rssi;
    uint16_t freq;
    uint8_t  valid;
  };

  #define FM_IOCTL_POWERUP            _IOWR (FM_IOC_MAGIC,  0, struct fm_tune_parm *)
  #define FM_IOCTL_POWERDOWN          _IOWR (FM_IOC_MAGIC,  1, int32_t *)
  #define FM_IOCTL_TUNE               _IOWR (FM_IOC_MAGIC,  2, struct fm_tune_parm *)
  #define FM_IOCTL_SETVOL             _IOWR (FM_IOC_MAGIC,  4, uint32_t *)
  #define FM_IOCTL_MUTE               _IOWR (FM_IOC_MAGIC,  6, uint32_t *)
  #define FM_IOCTL_GETRSSI            _IOWR (FM_IOC_MAGIC,  7, int32_t *)
  #define FM_IOCTL_GETCHIPID          _IOWR (FM_IOC_MAGIC, 10, uint16_t *)
  #define FM_IOCTL_GETMONOSTERO       _IOWR (FM_IOC_MAGIC, 13, uint16_t *)
  #define FM_IOCTL_RDS_ONOFF          _IOWR (FM_IOC_MAGIC, 18, uint16_t *)
  #define FM_IOCTL_RDS_SUPPORT        _IOWR (FM_IOC_MAGIC, 19, int32_t *)
  #define FM_IOCTL_IS_FM_POWERED_UP   _IOWR (FM_IOC_MAGIC, 24, uint32_t *)
  #define FM_IOCTL_ANA_SWITCH         _IOWR (FM_IOC_MAGIC, 30, int32_t *)
  #define FM_IOCTL_SETMONOSTERO       _IOWR (FM_IOC_MAGIC, 37, int32_t)
  #define FM_IOCTL_SOFT_MUTE_TUNE     _IOWR (FM_IOC_MAGIC, 63, struct fm_softmute_tune_t *)

  #define FM_BAND_UE          1                                         // 87.5 - 108 MHz
  #define FM_BAND_JAPANW      3                                         // 76   - 108 MHz
  #define FM_SPACE_100K       1
  #define FM_SPACE_200K       2

    // RDS data as the driver returns it from read (). Layout must match the kernel's 64-bit struct exactly.
  typedef struct {
    uint8_t  TP, TA, Music, Stereo, Artificial_Head, Compressed, Dynamic_PTY, Text_AB;
    uint32_t flag_status;
  } mtk_rds_flag_t;

  typedef struct {
    uint16_t Month, Day, Year, Hour, Minute;
    uint8_t  Local_Time_offset_signbit;
    uint8_t  Local_Time_offset_half_hour;
  } mtk_ct_t;

  typedef struct {
    int16_t  AF_Num;
    int16_t  AF [2][25];
    uint8_t  Addr_Cnt;
    uint8_t  isMethod_A;
    uint8_t  isAFNum_Get;
  } mtk_af_t;

  typedef struct {
    uint8_t  PS [4][8];
    uint8_t  Addr_Cnt;
  } mtk_ps_t;

  typedef struct {
    uint8_t  TextData [4][64];
    uint8_t  GetLength;
    uint8_t  isRTDisplay;
    uint8_t  TextLength;
    uint8_t  isTypeA;
    uint8_t  BufCnt;
    uint16_t Addr_Cnt;
  } mtk_rt_t;

  typedef struct {
    unsigned long total;
    unsigned long groupA [16];
    unsigned long groupB [16];
  } mtk_group_cnt_t;

  typedef struct {
    mtk_ct_t        CT;
    mtk_rds_flag_t  RDSFlag;
    uint16_t        PI;
    uint8_t         Switch_TP;
    uint8_t         PTY;
    mtk_af_t        AF_Data;
    mtk_af_t        AFON_Data;
    uint8_t         Radio_Page_Code;
    uint16_t        Program_Item_Number_Code;
    uint8_t         Extend_Country_Code;
    uint16_t        Language_Code;
    mtk_ps_t        PS_Data;
    uint8_t         PS_ON [8];
    mtk_rt_t        RT_Data;
    uint16_t        event_status;
    mtk_group_cnt_t gc;
  } mtk_rds_t;

  #define RDS_EVENT_PI_CODE         0x0002
  #define RDS_EVENT_PTY_CODE        0x0004
  #define RDS_EVENT_PROGRAMNAME     0x0008
  #define RDS_EVENT_LAST_RADIOTEXT  0x0040


    // Internal state:

  int dev_hndl = -1;

  pthread_t       rds_thread;
  int             rds_thread_running = 0;
  pthread_mutex_t rds_lock = PTHREAD_MUTEX_INITIALIZER;
  int             rds_dirty = 0;                                        // Set by the reader thread, consumed by rds_poll ()
  rds_struct_t    rds_pending;

  int mtk_ioctl (int req, void * arg, char * name) {
    errno = 0;
    int ret = ioctl (dev_hndl, req, arg);
    if (ret < 0)
      loge ("%s ret: %d  errno: %d (%s)", name, ret, errno, strerror (errno));
    else if (ena_log_tnr_extra)
      logd ("%s success", name);
    return (ret);
  }

  int mtk_band () {
    return (curr_freq_lo < 87500 ? FM_BAND_JAPANW : FM_BAND_UE);
  }

  int mtk_space () {
    return (curr_freq_inc >= 200 ? FM_SPACE_200K : FM_SPACE_100K);
  }

    // Copy fixed width RDS text, turning NULs and control characters into spaces and trimming trailing spaces.
  void rds_text_copy (char * dst, int dst_len, uint8_t * src, int src_len) {
    int idx, last = -1;
    for (idx = 0; idx < src_len && idx < dst_len - 1; idx ++) {
      char ch = src [idx];
      if (ch < 0x20 || ch == 0x7f)
        ch = ' ';
      dst [idx] = ch;
      if (ch != ' ')
        last = idx;
    }
    dst [last + 1] = 0;
  }

    // The driver's read () blocks until it has an RDS event, so it runs on its own thread.
  void * rds_reader (void * arg) {
    mtk_rds_t rds;
    logd ("rds_reader start  sizeof (mtk_rds_t): %d", (int) sizeof (rds));
    while (rds_thread_running && dev_hndl >= 0) {
      memset (& rds, 0, sizeof (rds));
      int len = read (dev_hndl, & rds, sizeof (rds));
      if (! rds_thread_running)
        break;
      if (len != sizeof (rds)) {
        if (len < 0 && errno != EINTR && errno != EAGAIN) {
          loge ("rds_reader read: %d  errno: %d (%s)", len, errno, strerror (errno));
          ms_sleep (200);
        }
        continue;
      }
      uint16_t evt = rds.event_status;
      if (! (evt & (RDS_EVENT_PI_CODE | RDS_EVENT_PTY_CODE | RDS_EVENT_PROGRAMNAME | RDS_EVENT_LAST_RADIOTEXT)))
        continue;

      pthread_mutex_lock (& rds_lock);
      if (evt & RDS_EVENT_PI_CODE)
        rds_pending.srds_pi = rds.PI;
      if (evt & RDS_EVENT_PTY_CODE)
        rds_pending.srds_pt = rds.PTY;
      if (evt & RDS_EVENT_PROGRAMNAME)
        rds_text_copy (rds_pending.srds_ps, sizeof (rds_pending.srds_ps), rds.PS_Data.PS [3], 8);
      if (evt & RDS_EVENT_LAST_RADIOTEXT)
        rds_text_copy (rds_pending.srds_rt, sizeof (rds_pending.srds_rt), rds.RT_Data.TextData [3], 64);
      rds_dirty = 1;
      pthread_mutex_unlock (& rds_lock);
    }
    logd ("rds_reader done");
    return (NULL);
  }

  void rds_reader_start () {
    if (rds_thread_running)
      return;
    rds_thread_running = 1;
    if (pthread_create (& rds_thread, NULL, rds_reader, NULL) != 0) {
      loge ("rds_reader_start pthread_create errno: %d", errno);
      rds_thread_running = 0;
    }
  }

  void rds_reader_stop () {
    if (! rds_thread_running)
      return;
    rds_thread_running = 0;                                             // Thread exits after the next read () returns; it is detached so we don't block here
    pthread_detach (rds_thread);
  }

  void rds_clear () {
    pthread_mutex_lock (& rds_lock);
    memset (& rds_pending, 0, sizeof (rds_pending));
    rds_pending.srds_pt = -1;
    rds_dirty = 0;
    pthread_mutex_unlock (& rds_lock);
    curr_rds_pi = 0;
    curr_rds_pt = -1;
    strlcpy (curr_rds_ps, "", sizeof (curr_rds_ps));
    strlcpy (curr_rds_rt, "", sizeof (curr_rds_rt));
  }

  int pwr_off () {
    rds_reader_stop ();
    if (dev_hndl >= 0) {
      int32_t type = 0;
      mtk_ioctl (FM_IOCTL_POWERDOWN, & type, "FM_IOCTL_POWERDOWN");
    }
    curr_state = 0;
    return (curr_state);
  }

  int seek_stop () {
    logd ("seek_stop");
    curr_seek_state = 0;
    return (curr_seek_state);
  }

  int band_set (int low , int high, int band) {
    logd ("band_set low: %d  high: %d  band: %d", low, high, band);
    return (band);
  }

  int freq_inc_set (int inc) {
    logd ("freq_inc_set inc: %d", inc);
    return (inc);
  }

  int emph75_set (int band) {                                           // De-emphasis is chosen by the driver from its band/region config
    return (band);
  }

  int rbds_set (int band) {
    return (band);
  }

  int tune_raw (int freq_khz) {                                         // Tune without touching Spirit state
    struct fm_tune_parm parm = {0};
    parm.band  = mtk_band ();
    parm.space = mtk_space ();
    parm.freq  = freq_khz / 100;
    int ret = mtk_ioctl (FM_IOCTL_TUNE, & parm, "FM_IOCTL_TUNE");
    if (ret == 0 && parm.err)
      loge ("FM_IOCTL_TUNE freq: %d  err: %d", freq_khz, parm.err);
    return (ret);
  }

  int station_valid (int freq_khz, int * rssi) {                        // The chip's own channel quality check (RSSI, multipath, softmute gain...)
    struct fm_softmute_tune_t smt = {0};
    smt.freq = freq_khz / 10;
    if (ioctl (dev_hndl, FM_IOCTL_SOFT_MUTE_TUNE, & smt) < 0)
      return (0);
    if (rssi)
      * rssi = smt.rssi;
    return (smt.valid);
  }


    // Chip API:

  int rds_poll (unsigned char * rds_grpd) {
    if (! rds_dirty)
      return (EVT_GET_NONE);

    rds_struct_t rds;
    pthread_mutex_lock (& rds_lock);
    rds = rds_pending;
    rds_dirty = 0;
    pthread_mutex_unlock (& rds_lock);

    curr_rds_pi = rds.srds_pi;
    curr_rds_pt = rds.srds_pt;
    strlcpy (curr_rds_ps, rds.srds_ps, sizeof (curr_rds_ps));
    strlcpy (curr_rds_rt, rds.srds_rt, sizeof (curr_rds_rt));
    strlcpy (conf_rds_ps, rds.srds_ps, sizeof (conf_rds_ps));
    strlcpy (conf_rds_rt, rds.srds_rt, sizeof (conf_rds_rt));
    conf_rds_pi = rds.srds_pi;
    conf_rds_pt = rds.srds_pt;
    if (cb_tuner_rds)
      cb_tuner_rds (& rds);
    return (EVT_GET_NONE);                                              // Already decoded by the driver; nothing raw for rds_group_process ()
  }

  int chip_imp_event_sg (unsigned char * rds_grpd) {
    return (rds_poll (rds_grpd));
  }

  int chip_imp_api_mode_sg (int api_mode) {
    if (api_mode == GET)
      return (curr_api_mode);
    curr_api_mode = api_mode;
    logd ("chip_imp_api_mode_sg curr_api_mode: %d", curr_api_mode);
    return (curr_api_mode);
  }

  int chip_imp_api_state_sg (int state) {
    logd ("chip_imp_api_state_sg state: %d", state);
    if (state == GET)
      return (curr_api_state);

    if (state == 0) {
      pwr_off ();
      if (dev_hndl >= 0)
        close (dev_hndl);
      dev_hndl = -1;
      curr_api_state = 0;
      return (curr_api_state);
    }

    errno = 0;
    dev_hndl = open ("/dev/fm", O_RDWR);
    if (dev_hndl < 0) {
      loge ("chip_imp_api_state_sg open /dev/fm errno: %d (%s)", errno, strerror (errno));
      curr_api_state = 0;
      return (curr_api_state);
    }
    uint16_t chip = 0;
    ioctl (dev_hndl, FM_IOCTL_GETCHIPID, & chip);
    logd ("chip_imp_api_state_sg /dev/fm: %d  chip id: 0x%x", dev_hndl, chip);

    curr_api_state = 1;
    return (curr_api_state);
  }

  int chip_imp_mode_sg (int mode) {
    if (mode == GET)
      return (curr_mode);
    curr_mode = mode;                                                   // Receive only; MediaTek connsys FM has no transmitter
    return (curr_mode);
  }

  int chip_imp_state_sg (int state) {
    if (state == GET)
      return (curr_state);

    logd ("chip_imp_state_sg state: %d", state);
    if (state == 0)
      return (pwr_off ());

    struct fm_tune_parm parm = {0};
    parm.band  = mtk_band ();
    parm.space = mtk_space ();
    parm.freq  = curr_freq_int / 100;
    if (mtk_ioctl (FM_IOCTL_POWERUP, & parm, "FM_IOCTL_POWERUP") < 0 || parm.err) {
      loge ("chip_imp_state_sg power up failed err: %d", parm.err);
      curr_state = 0;
      return (curr_state);
    }

    chip_imp_vol_sg (65535);
    chip_imp_rds_state_sg (curr_rds_state);

    curr_state = 1;
    logd ("chip_imp_state_sg curr_state: %d", curr_state);
    return (curr_state);
  }

  int chip_imp_antenna_sg (int antenna) {                               // 0 = wired headset antenna, 1 = short/internal antenna where the board has one
    if (antenna == GET)
      return (curr_antenna);
    int32_t ana = antenna;
    if (curr_state)
      mtk_ioctl (FM_IOCTL_ANA_SWITCH, & ana, "FM_IOCTL_ANA_SWITCH");
    curr_antenna = antenna;
    return (curr_antenna);
  }

  int chip_imp_band_sg (int band) {                                     //  0:EU    1:US    2:UU
    if (band == GET)
      return (curr_band);

    logd ("chip_imp_band_sg band: %d", band);
    curr_band = band;

    curr_freq_hi = 108000;
    curr_freq_lo = 87500;
    if (band == 2)                                                      // If Wide
      curr_freq_lo = 76000;

    curr_freq_inc = 100;
    if (band == 1)                                                      // If US
      curr_freq_inc = 200;

    band_set (curr_freq_lo, curr_freq_hi, band);
    freq_inc_set (curr_freq_inc);
    return (band);
  }

  int chip_imp_freq_sg (int freq) {                                     // kHz: 87500 - 108000
    if (freq == GET)
      return (curr_freq_int);

    logd ("chip_imp_freq_sg: %d", freq);
    if (tune_raw (freq) < 0)
      return (curr_freq_int);
    curr_freq_int = freq;
    rds_clear ();
    rds_init ();
    return (curr_freq_int);
  }

  int chip_imp_vol_sg (int vol) {                                       // Chip volume 0 - 15; real loudness is the Android stream volume
    if (vol == GET)
      return (curr_vol);
    uint32_t vol_reg = vol / 4096;
    if (vol && ! vol_reg)
      vol_reg = 1;
    if (vol_reg > 15)
      vol_reg = 15;
    if (curr_state || vol == 65535)
      mtk_ioctl (FM_IOCTL_SETVOL, & vol_reg, "FM_IOCTL_SETVOL");
    curr_vol = vol;
    return (curr_vol);
  }

  int chip_imp_thresh_sg (int thresh) {
    if (thresh == GET)
      return (curr_thresh);
    curr_thresh = thresh;
    return (curr_thresh);
  }

  int chip_imp_mute_sg (int mute) {
    if (mute == GET)
      return (curr_mute);
    uint32_t m = mute ? 1 : 0;
    mtk_ioctl (FM_IOCTL_MUTE, & m, "FM_IOCTL_MUTE");
    curr_mute = mute;
    return (curr_mute);
  }

  int chip_imp_softmute_sg (int softmute) {                             // Not exposed by the driver outside engineering mode
    if (softmute == GET)
      return (curr_softmute);
    curr_softmute = softmute;
    return (curr_softmute);
  }

  int chip_imp_stereo_sg (int stereo) {
    if (stereo == GET)
      return (curr_stereo);
    errno = 0;
    if (ioctl (dev_hndl, FM_IOCTL_SETMONOSTERO, stereo ? 0 : 1) < 0)   // Driver: 0 = auto stereo, 1 = forced mono
      loge ("FM_IOCTL_SETMONOSTERO errno: %d (%s)", errno, strerror (errno));
    curr_stereo = stereo;
    return (curr_stereo);
  }

  int chip_imp_seek_state_sg (int seek_state) {                         // Software seek using the chip's channel validity, as MediaTek's own HAL does
    if (seek_state == GET)
      return (curr_seek_state);

    if (seek_state == 0)
      return (seek_stop ());

    logd ("chip_imp_seek_state_sg seek_state: %d", seek_state);
    curr_seek_state = seek_state;
    int step = seek_state == 2 ? -curr_freq_inc : curr_freq_inc;
    int span = (curr_freq_hi - curr_freq_lo) / curr_freq_inc + 1;
    int freq = curr_freq_int;
    int found = 0, rssi = 0, ctr;

    chip_imp_mute_sg (1);
    for (ctr = 0; ctr < span && curr_seek_state; ctr ++) {
      freq += step;
      if (freq > curr_freq_hi)
        freq = curr_freq_lo;
      else if (freq < curr_freq_lo)
        freq = curr_freq_hi;
      if (station_valid (freq, & rssi)) {
        found = freq;
        break;
      }
    }
    logd ("chip_imp_seek_state_sg found: %d  rssi: %d  steps: %d", found, rssi, ctr);

    chip_imp_freq_sg (found ? found : curr_freq_int);
    chip_imp_mute_sg (0);
    curr_seek_state = 0;
    return (curr_freq_int);
  }

  int chip_imp_rds_state_sg (int rds_state) {
    if (rds_state == GET)
      return (curr_rds_state);

    if (dev_hndl >= 0) {
      int32_t supported = 0;
      ioctl (dev_hndl, FM_IOCTL_RDS_SUPPORT, & supported);
      uint16_t on = rds_state ? 1 : 0;
      if (supported)
        mtk_ioctl (FM_IOCTL_RDS_ONOFF, & on, "FM_IOCTL_RDS_ONOFF");
      if (rds_state && supported)
        rds_reader_start ();
      else
        rds_reader_stop ();
    }
    curr_rds_state = rds_state;
    return (curr_rds_state);
  }

  int chip_imp_rds_af_state_sg (int rds_af_state) {
    if (rds_af_state == GET)
      return (curr_rds_af_state);
    curr_rds_af_state = rds_af_state;
    return (curr_rds_af_state);
  }

  int chip_imp_rssi_sg (int fake_rssi) {                                // dBm (about -110 noise floor to -20); Spirit expects a positive level
    int32_t rssi = 0;
    if (ioctl (dev_hndl, FM_IOCTL_GETRSSI, & rssi) < 0)
      return (curr_rssi);
    curr_rssi = rssi + 110;
    if (curr_rssi < 0)
      curr_rssi = 0;
    return (curr_rssi);
  }

  int chip_imp_pilot_sg (int fake_pilot) {
    uint16_t ms = 0;
    if (ioctl (dev_hndl, FM_IOCTL_GETMONOSTERO, & ms) < 0)
      return (curr_pilot = 0);
    return (curr_pilot = ms ? 1 : 0);                                   // Driver: 1 = stereo
  }

  int chip_imp_rds_pi_sg (int rds_pi) {
    if (rds_pi == GET)
      return (curr_rds_pi);
    curr_rds_pi = rds_pi;
    return (curr_rds_pi);
  }

  int chip_imp_rds_pt_sg (int rds_pt) {
    if (rds_pt == GET)
      return (curr_rds_pt);
    curr_rds_pt = rds_pt;
    return (curr_rds_pt);
  }

  char * chip_imp_rds_ps_sg (char * rds_ps) {
    if (rds_ps == GETP)
      return (curr_rds_ps);
    strlcpy (curr_rds_ps, rds_ps, sizeof (curr_rds_ps));
    return (curr_rds_ps);
  }

  char * chip_imp_rds_rt_sg (char * rds_rt) {
    if (rds_rt == GETP)
      return (curr_rds_rt);
    strlcpy (curr_rds_rt, rds_rt, sizeof (curr_rds_rt));
    return (curr_rds_rt);
  }

  char * chip_imp_extension_sg (char * reg) {
    if (reg == GETP)
      return (curr_extension);
    strlcpy (curr_extension, reg, sizeof (curr_extension));
    return (curr_extension);
  }

