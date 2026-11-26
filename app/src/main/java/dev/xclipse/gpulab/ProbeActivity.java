package dev.xclipse.gpulab;
import android.app.Activity;
import android.os.Bundle;
import android.widget.TextView;
import android.content.Intent;
import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
public class ProbeActivity extends Activity {
 @Override public void onCreate(Bundle b){super.onCreate(b); TextView t=new TextView(this);t.setText("Executando consulta em processo separado...");setContentView(t);
 int mode=getIntent().getIntExtra("mode",0);String name=getIntent().getStringExtra("name");
 new Thread(()->{try{String report=Native.read(mode,name==null?"":name);Files.write(new File(getFilesDir(),"native-report.txt").toPath(),report.getBytes(StandardCharsets.UTF_8));runOnUiThread(()->{setResult(RESULT_OK);finish();});}catch(Throwable e){runOnUiThread(()->{setResult(RESULT_CANCELED,new Intent().putExtra("error",e.toString()));finish();});}},"gpu-probe").start();
 }
}
