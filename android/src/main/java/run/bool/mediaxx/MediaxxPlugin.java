package run.bool.mediaxx;

import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Objects;
import java.io.InputStream;
import java.io.BufferedReader;
import java.util.regex.Pattern;
import java.io.InputStreamReader;
import java.util.zip.GZIPInputStream;

import android.util.Log;
import androidx.annotation.NonNull;
import android.content.res.AssetManager;

import io.flutter.embedding.engine.plugins.FlutterPlugin;

import run.bool.mediaxxandroidhelper.MediaxxAndroidHelper;

/** MediaKitLibsAndroidVideoPlugin */
public class MediaxxPlugin implements FlutterPlugin {

    @Override
    public void onAttachedToEngine(@NonNull FlutterPluginBinding flutterPluginBinding) {
        Log.i("mediaxx", "package:mediaxx attached.");
        try {
            MediaxxAndroidHelper.setApplicationContextJava(flutterPluginBinding.getApplicationContext());
            Log.i("mediaxx", "Saved application context.");
        } catch (Throwable e) {
            e.printStackTrace();
        }
    }

    @Override
    public void onDetachedFromEngine(@NonNull FlutterPluginBinding binding) {
        Log.i("mediaxx", "package:mediaxx detached.");
    }
}
