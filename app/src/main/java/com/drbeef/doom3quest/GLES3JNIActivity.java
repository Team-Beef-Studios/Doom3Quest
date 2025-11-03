
package com.drbeef.doom3quest;


import android.Manifest;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.os.RemoteException;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.util.Log;
import android.util.Pair;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.WindowManager;

import com.drbeef.externalhapticsservice.HapticServiceClient;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Vector;

import com.drbeef.externalhapticsservice.HapticsConstants;

import static android.system.Os.setenv;

@SuppressLint("SdCardPath") public class GLES3JNIActivity extends Activity implements SurfaceHolder.Callback
{

	private Vector<HapticServiceClient> externalHapticsServiceClients = new Vector<>();

	//Use a vector of pairs, it is possible a given package _could_ in the future support more than one haptic service
	//so a map here of Package -> Action would not work.
	private static Vector<Pair<String, String>> externalHapticsServiceDetails = new Vector<>();

	private static boolean started = false;

	static
	{
		System.loadLibrary( "doom3" );

		//Add possible external haptic service details here
		externalHapticsServiceDetails.add(Pair.create(HapticsConstants.BHAPTICS_PACKAGE, HapticsConstants.BHAPTICS_ACTION_FILTER));
		externalHapticsServiceDetails.add(Pair.create(HapticsConstants.FORCETUBE_PACKAGE, HapticsConstants.FORCETUBE_ACTION_FILTER));
	}

	private int permissionAttempt = 0;
	private static final int READ_EXTERNAL_STORAGE_PERMISSION_ID = 1;
	private static final int WRITE_EXTERNAL_STORAGE_PERMISSION_ID = 2;


	private static final String APPLICATION = "Doom3Quest";

	private String commandLineParams;

	private SurfaceView mView;
	private SurfaceHolder mSurfaceHolder;
	private long mNativeHandle;


	public void shutdown() {
		finish();
		System.exit(0);
	}


	public void haptic_event(String event, int position, int flags, int intensity, float angle, float yHeight)  {

		for (HapticServiceClient externalHapticsServiceClient : externalHapticsServiceClients) {

			if (externalHapticsServiceClient.hasService()) {
				try {
					externalHapticsServiceClient.getHapticsService().hapticEvent(APPLICATION, event, position, flags, intensity, angle, yHeight);
				}
				catch (RemoteException r)
				{
					Log.v(APPLICATION, r.toString());
				}
			}
		}
	}

	public void haptic_updateevent(String event, int intensity, float angle) {

		for (HapticServiceClient externalHapticsServiceClient : externalHapticsServiceClients) {

			if (externalHapticsServiceClient.hasService()) {
				try {
					externalHapticsServiceClient.getHapticsService().hapticUpdateEvent(APPLICATION, event, intensity, angle);
				} catch (RemoteException r) {
					Log.v(APPLICATION, r.toString());
				}
			}
		}
	}

	public void haptic_stopevent(String event) {

		for (HapticServiceClient externalHapticsServiceClient : externalHapticsServiceClients) {

			if (externalHapticsServiceClient.hasService()) {
				try {
					externalHapticsServiceClient.getHapticsService().hapticStopEvent(APPLICATION, event);
				} catch (RemoteException r) {
					Log.v(APPLICATION, r.toString());
				}
			}
		}
	}

	public void haptic_endframe() {

		for (HapticServiceClient externalHapticsServiceClient : externalHapticsServiceClients) {

			if (externalHapticsServiceClient.hasService()) {
				try {
					externalHapticsServiceClient.getHapticsService().hapticFrameTick();
				} catch (RemoteException r) {
					Log.v(APPLICATION, r.toString());
				}
			}
		}
	}

	public void haptic_enable() {

		for (HapticServiceClient externalHapticsServiceClient : externalHapticsServiceClients) {

			if (externalHapticsServiceClient.hasService()) {
				try {
					externalHapticsServiceClient.getHapticsService().hapticEnable();
				} catch (RemoteException r) {
					Log.v(APPLICATION, r.toString());
				}
			}
		}
	}

	public void haptic_disable() {

		for (HapticServiceClient externalHapticsServiceClient : externalHapticsServiceClients) {

			if (externalHapticsServiceClient.hasService()) {
				try {
					externalHapticsServiceClient.getHapticsService().hapticDisable();
				} catch (RemoteException r) {
					Log.v(APPLICATION, r.toString());
				}
			}
		}
	}

	@Override protected void onCreate( Bundle icicle )
	{
		super.onCreate( icicle );

		mView = new SurfaceView( this );
		setContentView( mView );
		mView.getHolder().addCallback( this );

		// Force the screen to stay on, rather than letting it dim and shut off
		// while the user is watching a movie.
		getWindow().addFlags( WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON );

		// Force screen brightness to stay at maximum
		WindowManager.LayoutParams params = getWindow().getAttributes();
		params.screenBrightness = 1.0f;
		getWindow().setAttributes( params );

		if (!started) {
			checkPermissionsAndInitialize();
			started = true;
		}
	}

	/** Initializes the Activity only if the permission has been granted. */
	private void checkPermissionsAndInitialize() {
		// Boilerplate for checking runtime permissions in Android.
		if (ContextCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE)
				!= PackageManager.PERMISSION_GRANTED){
			ActivityCompat.requestPermissions(this,
					new String[]{Manifest.permission.READ_EXTERNAL_STORAGE,
							Manifest.permission.WRITE_EXTERNAL_STORAGE},
					WRITE_EXTERNAL_STORAGE_PERMISSION_ID);
		}
		else
		{
			// Permissions have already been granted.
			create();
		}
	}

	/** Handles the user accepting the permission. */
	@Override
	public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] results) {
		if (requestCode == WRITE_EXTERNAL_STORAGE_PERMISSION_ID) {
			permissionAttempt++;
			if (permissionAttempt < 5) {
				checkPermissionsAndInitialize();
			} else {
				System.exit(0);
			}
		}
	}

	public void create() {

		//Define paths
		File root = new File("/sdcard/Doom3Quest");
		File base = new File(root, "base");
		File cdoom = new File(root, "cdoom");
		File doom2 = new File(root, "Doom_II_for_Doom_3_unofficial");
		File roe = new File(root, "d3xp");
		File lm = new File(root, "d3le");

		boolean exitAfterCopy = false;

		//If this is first run on clean system, or user hasn't copied anything yet, just exit after we have copied
		if (!new File(base, "pak000.pk4").exists())
		{
			exitAfterCopy = true;
		}

		//Create file structure
		base.mkdirs();
		copy_common_assets(base);
		copy_asset(root.getAbsolutePath(), "commandline.txt", false);
		copy_optional_assets(cdoom, "pak399cd.pk4");
		copy_optional_assets(doom2, "pak399d2.pk4");
		copy_optional_assets(roe, "pak399roe.pk4");
		copy_optional_assets(lm, "pak399lm.pk4");

		//delete incompatible files
		if (cdoom.exists()) {
			new File(cdoom, "Xpak400.pk4").delete();
		}
		if (lm.exists()) {
			for (int i = 0; i < 10; i++) {
				new File(lm, "pak00" + i + ".pk4").delete();
			}
		}

		if (exitAfterCopy)
		{
			finish();
			System.exit(0);
		}

		//Copy save games from old version to new version
		File savesOld = new File(root, "saves");
		File savesNew = new File(root, "saves_v1.3");
		ArrayList<Pair<File, File>> toCopy = new ArrayList<>();
		toCopy.add(new Pair<>(savesOld, savesNew));
		while (!toCopy.isEmpty()) {
			savesOld = toCopy.get(0).first;
			savesNew = toCopy.get(0).second;
			toCopy.remove(0);
			if (savesOld.isDirectory()) {
				for (File file : savesOld.listFiles()) {
					File targetFile = new File(savesNew, file.getName());
					if (targetFile.isDirectory() || !targetFile.exists()) {
						toCopy.add(new Pair<>(file, targetFile));
					}
				}
				savesNew.mkdirs();
			} else {
				try {
					copy_stream(new FileInputStream(savesOld), new FileOutputStream(savesNew));
					savesNew.setLastModified(savesOld.lastModified());
				} catch (IOException e) {
					e.printStackTrace();
				}
			}
		}


		//Read these from a file and pass through
		commandLineParams = new String("doom3quest");

		//See if user is trying to use command line params
		File commandline = new File(root.getAbsolutePath(), "commandline.txt");
		if(commandline.exists()) // should exist now!
		{
			BufferedReader br;
			try {
				br = new BufferedReader(new FileReader(commandline.getAbsolutePath()));
				String s;
				StringBuilder sb=new StringBuilder(0);
				while ((s=br.readLine())!=null)
					sb.append(s + " ");
				br.close();

				commandLineParams = new String(sb.toString());
			} catch (FileNotFoundException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			} catch (IOException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
		}

		try {
			ApplicationInfo ai =  getApplicationInfo();

			setenv("USER_FILES", root.getAbsolutePath(), true);
			setenv("GAMELIBDIR", getApplicationInfo().nativeLibraryDir, true);
			setenv("GAMETYPE", "16", true); // hard coded for now
		}
		catch (Exception e)
		{
			e.printStackTrace();
		}

		for (Pair<String, String> serviceDetail : externalHapticsServiceDetails) {
			HapticServiceClient client = new HapticServiceClient(this, (state, desc) -> {
				Log.v(APPLICATION, "ExternalHapticsService " + serviceDetail.second + ": " + desc);
			}, new Intent(serviceDetail.second)
					.setPackage(serviceDetail.first));

			client.bindService();
			externalHapticsServiceClients.add(client);
		}

		mNativeHandle = GLES3JNILib.onCreate( this, commandLineParams );
	}

	public void copy_common_assets(File path) {
		if (path.exists()) {
			copy_asset(path.getAbsolutePath(), "pak399.pk4", true);
			copy_asset(path.getAbsolutePath(), "quest1_default.cfg", true);
			copy_asset(path.getAbsolutePath(), "quest2_default.cfg", true);
			copy_asset(path.getAbsolutePath(), "quest3_default.cfg", true);
		}
	}

	public void copy_optional_assets(File path, String file) {
		if (path.exists()) {
			copy_common_assets(path);
			copy_asset(path.getAbsolutePath(), file, true);
		}
	}

	public void copy_asset(String path, String name, boolean force) {
		File f = new File(path + "/" + name);
		if (!f.exists() || force) {
			
			//Ensure we have an appropriate folder
			String fullname = path + "/" + name;
			String directory = fullname.substring(0, fullname.lastIndexOf("/"));
			new File(directory).mkdirs();
			_copy_asset(name, path + "/" + name);
		}
	}

	public void _copy_asset(String name_in, String name_out) {
		AssetManager assets = this.getAssets();

		try {
			InputStream in = assets.open(name_in);
			OutputStream out = new FileOutputStream(name_out);

			copy_stream(in, out);

			out.close();
			in.close();

		} catch (Exception e) {

			e.printStackTrace();
		}

	}

	public static void copy_stream(InputStream in, OutputStream out)
			throws IOException {
		byte[] buf = new byte[1024];
		while (true) {
			int count = in.read(buf);
			if (count <= 0)
				break;
			out.write(buf, 0, count);
		}
	}

	@Override protected void onStart()
	{
		Log.v(APPLICATION, "GLES3JNIActivity::onStart()" );
		super.onStart();

		if ( mNativeHandle != 0 )
		{
			GLES3JNILib.onStart(mNativeHandle, this);
		}
	}

	@Override protected void onResume()
	{
		Log.v(APPLICATION, "GLES3JNIActivity::onResume()" );
		super.onResume();

		if ( mNativeHandle != 0 )
		{
			GLES3JNILib.onResume(mNativeHandle);
		}
	}

	@Override protected void onPause()
	{
		Log.v(APPLICATION, "GLES3JNIActivity::onPause()" );
		if ( mNativeHandle != 0 )
		{
			GLES3JNILib.onPause(mNativeHandle);
		}
		super.onPause();
	}

	@Override protected void onStop()
	{
		Log.v(APPLICATION, "GLES3JNIActivity::onStop()" );
		if ( mNativeHandle != 0 )
		{
			GLES3JNILib.onStop(mNativeHandle);
		}


		super.onStop();
	}

	@Override protected void onDestroy()
	{
		Log.v(APPLICATION, "GLES3JNIActivity::onDestroy()" );

		try {
			//The destructors take too long to unload, let's use exit(0)
			/*if ( mSurfaceHolder != null )
			{
				GLES3JNILib.onSurfaceDestroyed( mNativeHandle );
			}

			if ( mNativeHandle != 0 )
			{
				GLES3JNILib.onDestroy(mNativeHandle);
			}*/

			for (HapticServiceClient externalHapticsServiceClient : externalHapticsServiceClients) {
				externalHapticsServiceClient.stopBinding();
			}
		} catch (Exception e) {
			e.printStackTrace();
		}

		System.exit(0);
		super.onDestroy();
		mNativeHandle = 0;
	}

	@Override public void surfaceCreated( SurfaceHolder holder )
	{
		Log.v(APPLICATION, "GLES3JNIActivity::surfaceCreated()" );
		if ( mNativeHandle != 0 )
		{
			GLES3JNILib.onSurfaceCreated( mNativeHandle, holder.getSurface() );
			mSurfaceHolder = holder;
		}
	}

	@Override public void surfaceChanged( SurfaceHolder holder, int format, int width, int height )
	{
		Log.v(APPLICATION, "GLES3JNIActivity::surfaceChanged()" );
		if ( mNativeHandle != 0 )
		{
			GLES3JNILib.onSurfaceChanged( mNativeHandle, holder.getSurface() );
			mSurfaceHolder = holder;
		}
	}
	
	@Override public void surfaceDestroyed( SurfaceHolder holder )
	{
		Log.v(APPLICATION, "GLES3JNIActivity::surfaceDestroyed()" );
		if ( mNativeHandle != 0 )
		{
			GLES3JNILib.onSurfaceDestroyed( mNativeHandle );
			mSurfaceHolder = null;
		}
	}
}
