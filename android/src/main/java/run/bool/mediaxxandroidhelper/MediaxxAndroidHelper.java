package run.bool.mediaxxandroidhelper;

import android.net.Uri;
import android.content.Context;

public class MediaxxAndroidHelper {
    static {
        System.loadLibrary("mediaxx");
    }

    // Store android.content.Context for access in openFileDescriptor.
    private static Context applicationContext = null;

    private static native void setApplicationContextNative(Context context);

    public static void setApplicationContextJava(Context context) {
        applicationContext = context;
        setApplicationContextNative(context);
    }

}
