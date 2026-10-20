package fm.a2d.sf;

import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.AudioTrack;

import java.io.File;
import java.lang.reflect.Method;

  // Root audio helper for MediaTek FM: started by svc_aud as   su -c "CLASSPATH=<apk> app_process /system/bin fm.a2d.sf.rootaudio <run_file> <speaker_file>"

  // Android only lets privileged apps or root capture MediaRecorder.AudioSource.RADIO_TUNER (1998). Running here as root avoids
  // needing Spirit installed as a priv-app. It copies tuner PCM to a STREAM_MUSIC AudioTrack until <run_file> disappears,
  // and forces media to the loudspeaker while <speaker_file> exists (the wired headset is often only the antenna).

public class rootaudio {

  private static final int    SRC_RADIO_TUNER = 1998;
  private static final int    SAMPLE_RATE     = 48000;                  // MediaTek HAL forces FM_TUNER input to 48 kHz
  private static final int    FOR_MEDIA       = 1;                      // AudioSystem.FOR_MEDIA
  private static final int    FORCE_NONE      = 0;
  private static final int    FORCE_SPEAKER   = 1;

  private static Method force_use = null;

  private static void force_use_set (int config) {
    try {
      if (force_use == null)
        force_use = Class.forName ("android.media.AudioSystem").getMethod ("setForceUse", int.class, int.class);
      force_use.invoke (null, FOR_MEDIA, config);
    }
    catch (Throwable e) {
      System.err.println ("setForceUse: " + e);
    }
  }

  public static void main (String [] args) {
    File run_file     = new File (args [0]);
    File speaker_file = new File (args [1]);

    int buf_size = Math.max (AudioRecord.getMinBufferSize (SAMPLE_RATE, AudioFormat.CHANNEL_IN_STEREO, AudioFormat.ENCODING_PCM_16BIT) * 2, 16384);
    AudioRecord rec = new AudioRecord (SRC_RADIO_TUNER, SAMPLE_RATE, AudioFormat.CHANNEL_IN_STEREO, AudioFormat.ENCODING_PCM_16BIT, buf_size);
    if (rec.getState () != AudioRecord.STATE_INITIALIZED) {
      System.err.println ("rootaudio: RADIO_TUNER capture not available");
      System.exit (1);
    }
    AudioTrack trk = new AudioTrack (AudioManager.STREAM_MUSIC, SAMPLE_RATE, AudioFormat.CHANNEL_OUT_STEREO, AudioFormat.ENCODING_PCM_16BIT, buf_size, AudioTrack.MODE_STREAM);

    boolean speaker = speaker_file.exists ();
    force_use_set (speaker ? FORCE_SPEAKER : FORCE_NONE);

    rec.startRecording ();
    trk.play ();

    short [] pcm = new short [buf_size / 4];                              // ~1/4 buffer per read keeps run_file checks frequent
    long next_check_ms = 0;
    while (true) {
      int len = rec.read (pcm, 0, pcm.length);
      if (len > 0)
        trk.write (pcm, 0, len);

      long now = System.currentTimeMillis ();
      if (now >= next_check_ms) {                                       // Poll the control files a few times a second
        next_check_ms = now + 250;
        if (! run_file.exists ())
          break;
        boolean want_speaker = speaker_file.exists ();
        if (want_speaker != speaker) {
          speaker = want_speaker;
          force_use_set (speaker ? FORCE_SPEAKER : FORCE_NONE);
        }
      }
    }

    trk.stop ();
    trk.release ();
    rec.stop ();
    rec.release ();
    if (speaker)
      force_use_set (FORCE_NONE);
    System.exit (0);                                                    // app_process otherwise lingers on binder threads
  }
}
