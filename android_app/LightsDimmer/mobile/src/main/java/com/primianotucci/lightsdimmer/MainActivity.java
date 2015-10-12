package com.primianotucci.lightsdimmer;

import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;
import android.support.design.widget.FloatingActionButton;
import android.support.design.widget.Snackbar;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.Toolbar;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.Button;
import android.widget.SeekBar;
import android.widget.TextView;

import org.w3c.dom.Text;

public class MainActivity extends AppCompatActivity implements  LightDimmerBeacon.Listener, LightTouchView.Listener {
    LightDimmerBeacon mDimmerBeacon;
    Handler mHandler;
    LightTouchView mLightView;
    boolean didReadInitialValues;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        Toolbar toolbar = (Toolbar) findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);

        mHandler = new Handler();
        mDimmerBeacon = new LightDimmerBeacon(this, this);
        mLightView = (LightTouchView) findViewById(R.id.lightTouchView);
        mLightView.setListener(this);
    }

    @Override
    protected void onResume() {
        super.onResume();
        mDimmerBeacon.Start();
    }

    private void resetAll() {
        for (int i = 0; i < 4; ++i)
            mDimmerBeacon.SetBrightness(i, 0);
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
        if (id == R.id.action_reset) {
            resetAll();
            return true;
        }

        return super.onOptionsItemSelected(item);
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
        if (didReadInitialValues)
             return;
        didReadInitialValues = true;
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                assert (values.length == 4);
                for (int i = 0; i < values.length; ++i) {
                    mLightView.setChannelValue(i, values[i] / 31.0f);
                }
            }
        });
    }

    @Override
    public void onStateChange(final String state) {
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                setTitle(state);
            }
        });
    }

    @Override
    public void OnValueChange(int channel, float value) {
        int rawValue = (int) (value * 31);
        Log.e("lights", "ValueChange " + channel + " value " + rawValue);
        mDimmerBeacon.SetBrightness(channel, rawValue);
    }

    @Override
    public void OnVelocityChange(int channel, float velocity) {
        int rawVelocity = (int) (velocity * 31);
        Log.e("lights", "SmoothingChange " + channel + " value " + rawVelocity);
        mDimmerBeacon.SetSmoothing(channel, rawVelocity);
    }

}
