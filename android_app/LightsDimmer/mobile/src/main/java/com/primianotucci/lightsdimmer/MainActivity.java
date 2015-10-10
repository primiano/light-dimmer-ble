package com.primianotucci.lightsdimmer;

import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;
import android.support.design.widget.FloatingActionButton;
import android.support.design.widget.Snackbar;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.Toolbar;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.Button;
import android.widget.SeekBar;
import android.widget.TextView;

import org.w3c.dom.Text;

public class MainActivity extends AppCompatActivity implements SeekBar.OnSeekBarChangeListener, LightDimmerBeacon.Listener {
    LightDimmerBeacon mDimmerBeacon;
    TextView mStatusLog;
    Handler mHandler;
    SeekBar[] mSeekBars;
    SeekBar mSeekBarSmoothing;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        Toolbar toolbar = (Toolbar) findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);

        mHandler = new Handler();
        mDimmerBeacon = new LightDimmerBeacon(this, this);
        mStatusLog = (TextView) findViewById(R.id.statusLog);
        mSeekBars = new SeekBar[4];
        mSeekBars[0] = ((SeekBar) findViewById(R.id.seekBar));
        mSeekBars[1] = ((SeekBar) findViewById(R.id.seekBar2));
        mSeekBars[2] = ((SeekBar) findViewById(R.id.seekBar3));
        mSeekBars[3] = ((SeekBar) findViewById(R.id.seekBar4));
        for (SeekBar seekBar : mSeekBars)
            seekBar.setOnSeekBarChangeListener(this);
        mSeekBarSmoothing = ((SeekBar) findViewById(R.id.seekBarSmoothing));
        mSeekBarSmoothing.setOnSeekBarChangeListener(this);
        ((Button) findViewById(R.id.btReset)).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                for (int i = 0; i < 4; ++i)
                    mDimmerBeacon.SetBrightness(i, 0);
            }
        });
        mDimmerBeacon.Start();
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
        getMenuInflater().inflate(R.menu.menu_main, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle action bar item clicks here. The action bar will
        // automatically handle clicks on the Home/Up button, so long
        // as you specify a parent activity in AndroidManifest.xml.
        int id = item.getItemId();

        //noinspection SimplifiableIfStatement
        if (id == R.id.action_settings) {
            return true;
        }

        return super.onOptionsItemSelected(item);
    }

    @Override
    public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
        if (!fromUser)
            return;
        if (seekBar.getId() == R.id.seekBarSmoothing) {
            for (int i = 0; i < 4; ++i)
                mDimmerBeacon.SetSmoothing(i, progress);
            return;
        }

        int channel = 0;
        if (seekBar.getId() == R.id.seekBar)
            channel = 0;
        else if (seekBar.getId() == R.id.seekBar2)
            channel = 1;
        else if (seekBar.getId() == R.id.seekBar3)
            channel = 2;
        else if (seekBar.getId() == R.id.seekBar4)
            channel = 3;
        mDimmerBeacon.SetBrightness(channel, progress);
    }

    @Override
    public void onStartTrackingTouch(SeekBar seekBar) {

    }

    @Override
    public void onStopTrackingTouch(SeekBar seekBar) {
    }

    @Override
    public void onDeviceConnected(final String name) {
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                setTitle(name);
            }
        });
    }

    @Override
    public void onConnectionLost() {
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                setTitle("DISCONNECTED");
            }
        });
    }

    @Override
    public void onReadValues(final int[] values) {
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                assert (values.length == 4);
                for (int i = 0; i < values.length; ++i) {
                    mSeekBars[i].setProgress(values[i]);
                }
            }
        });
    }

    @Override
    public void onStateChange(final String state) {
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                mStatusLog.setText(state);
            }
        });
    }
}
