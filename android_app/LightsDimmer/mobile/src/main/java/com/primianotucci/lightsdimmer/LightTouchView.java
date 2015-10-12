package com.primianotucci.lightsdimmer;

import android.content.Context;
import android.content.res.TypedArray;
import android.gesture.Gesture;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.drawable.Drawable;
import android.text.TextPaint;
import android.util.AttributeSet;
import android.util.Log;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.View;

/**
 * TODO: document your custom view class.
 */
public class LightTouchView extends View implements GestureDetector.OnGestureListener {
    private GestureDetector mGestureDetector;
    private float mBrightness[] = new float[4];
    private final static String TAG = "lights";
    private final static float INITIAL_VELOCITY = 0.1f;
    private Listener mListener;

    public interface Listener {
        public void OnValueChange(int channel, float value);
        public void OnVelocityChange(int channel, float velocity);
    }

    public void setListener(Listener listener) {
        mListener = listener;
    }

    public void setChannelValue(int channel, float value) {
        mBrightness[channel] = Math.min(Math.max(value, 0.0f), 1.0f);
        invalidate();
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        mGestureDetector.onTouchEvent(event);
        return true;
    }

    public LightTouchView(Context context) {
        super(context);
        init();
    }

    public LightTouchView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    public LightTouchView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        init();
    }

    private void init() {
        mGestureDetector = new GestureDetector(getContext(), this);
        for (int i = 0 ; i < mBrightness.length; ++i)
            mBrightness[i] = 0;
    }

    private int getChannel(MotionEvent e) {
        return Math.min((int) (e.getX() / (getWidth() / mBrightness.length)),
                        mBrightness.length - 1);
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        float quarter = getWidth() / 4;
        final int colors[] = { 0xffe91e63, 0xff3f51b5, 0xff8bc34a, 0xffffc107 };
        String txt;
        for (int i=0; i < 4; ++i) {
            Paint paint = new Paint();
            paint.setStyle(Paint.Style.FILL);
            paint.setColor(colors[i]);
            canvas.drawRect(i * quarter, 0, (i + 1) * quarter, getHeight(), paint);
            txt = Integer.toString((int) (mBrightness[i] * 100)) + "%";
            paint.setColor(Color.WHITE);
            paint.setTextSize(40.0f);
            canvas.drawText(txt, i * quarter + quarter / 4, getHeight() - 20, paint);
        }
    }

    @Override
    public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {
        int channel = getChannel(e2);
        float prevValue = mBrightness[channel];
        float value = mBrightness[channel] + (distanceY / getHeight());
        value = Math.max(Math.min(value, 1.0f), 0.0f);
        mBrightness[channel] = value;
        //Log.i(TAG, "scroll ch: " + channel + " " + prevValue + " -> " + value + "   delta " + distanceY);
        invalidate();
        if (mListener != null)
            mListener.OnValueChange(channel, value);
        return true;
    }

    @Override
    public boolean onFling(MotionEvent e1, MotionEvent e2, float velocityX, float velocityY) {
        //Log.d(TAG, "Fling " + velocityX + " y " + velocityY);
        if (mListener != null)
            mListener.OnVelocityChange(getChannel(e2), Math.max(Math.min(Math.abs(velocityY) / 7000f, 1.0f), 0.5f));
        return true;
    }

    @Override
    public boolean onDown(MotionEvent e) {
        if (mListener != null)
            mListener.OnVelocityChange(getChannel(e), INITIAL_VELOCITY);
        return true;
    }

    @Override
    public void onShowPress(MotionEvent e) {

    }

    @Override
    public boolean onSingleTapUp(MotionEvent e) {
        return false;
    }

    @Override
    public void onLongPress(MotionEvent e) {

    }
}
