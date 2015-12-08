package com.cryptopp.prng;

import android.app.Activity;
import android.graphics.Typeface;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Build.VERSION;
import android.os.Bundle;
import android.os.StrictMode;
import android.util.DisplayMetrics;
import android.view.View;
import android.widget.TextView;

import com.cryptopp.prng.R;
import com.cryptopp.prng.PRNG;

public class MainActivity extends Activity {
	/** Called when the activity is first created. */
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.main);

		// ///////////////////////////////////////////////////////////

		final TextView lblNumbers = (TextView) findViewById(R.id.lblNumbers);
		if (lblNumbers != null) {
			lblNumbers.setTypeface(Typeface.MONOSPACE);
		}

		// ///////////////////////////////////////////////////////////

		StrictMode.VmPolicy.Builder builder = new StrictMode.VmPolicy.Builder();
		builder.detectLeakedSqlLiteObjects();

		if (VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB) {
			builder.detectAll();
		}

		builder.penaltyLog();

		StrictMode.VmPolicy vmp = builder.build();
		StrictMode.setVmPolicy(vmp);

		PRNG prng = new PRNG();
		byte[] bytes = new byte[32];
		prng.getBytes(bytes);
	}

	public void btnReseed_onClick(View view) {

		new AsyncTask<Void, Void, Void>() {
			@Override
			protected Void doInBackground(Void... params) {

				final TextView lblNumbers = (TextView) findViewById(R.id.lblNumbers);
				if (lblNumbers != null) {
					byte[] seed = lblNumbers.getText().toString().getBytes();

					if (seed != null)
						PRNG.Reseed(seed);

					seed = null;
				}
				return null;
			}
		}.execute();
	}

	public void btnGenerate_onClick(View view) {

		new AsyncTask<Void, Void, String>() {
			@Override
			protected String doInBackground(Void... params) {

				Float pixelWidth = 0.0f;
				DisplayMetrics dm = getBaseContext().getResources()
						.getDisplayMetrics();
				if (dm != null) {
					pixelWidth = (float) dm.widthPixels;
				}

				Float charWidth = 0.0f;
				final TextView lblNumbers = (TextView) findViewById(R.id.lblNumbers);
				if (lblNumbers != null) {
					charWidth = lblNumbers.getPaint().measureText(" ");
				}

				/* The extra gyrations negate Math.round's rounding up */
				int charPerLine = Math.round(pixelWidth - 0.5f)
						/ Math.round(charWidth - 0.5f);
				if (charPerLine == 0)
					charPerLine = 21;

				/* This prints about 4 lines of random numbers */
				byte[] bytes = new byte[(charPerLine / 3) * 4];
				PRNG.GetBytes(bytes);

				StringBuilder sb = new StringBuilder();
				for (byte b : bytes)
					sb.append(String.format("%02X ", (0xff & b)));

				bytes = null;
				dm = null;

				return sb.toString();
			}

			@Override
			protected void onPostExecute(String result) {

				final TextView lblNumbers = (TextView) findViewById(R.id.lblNumbers);
				if (lblNumbers != null) {
					lblNumbers.setText(result);
				}
			}
		}.execute();
	}
}
