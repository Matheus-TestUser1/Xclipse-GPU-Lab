package dev.xclipse.gpulab;
import java.nio.charset.StandardCharsets;
final class Native {
 static { System.loadLibrary("gpulab"); }
 static native byte[] scan(int mode,String name);
 static String read(int mode,String name){return new String(scan(mode,name),StandardCharsets.UTF_8);}
}
