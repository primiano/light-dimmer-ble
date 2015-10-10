package com.primianotucci.lightsdimmer;

import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothManager;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.content.Context;
import android.os.Handler;
import android.os.ParcelUuid;
import android.util.Log;

import java.util.ArrayList;
import java.util.LinkedList;
import java.util.List;
import java.util.UUID;

public class LightDimmerBeacon {
    private BluetoothLeScanner mBluetoothScanner;
    private Context mContext;
    private BluetoothGatt mGatt;
    private BluetoothGattCharacteristic mCharacteristic;
    private Listener mListener;
    private LinkedList<byte[]> mCommandQueue;

    private static final UUID SERVICE_UUID = UUID.fromString("0000ffe0-0000-1000-8000-00805f9b34fb");
    private static final UUID CHARACTERISTC_UUID = UUID.fromString("0000ffe1-0000-1000-8000-00805f9b34fb");

    public interface Listener {
        public void onDeviceConnected(String name);
        public void onConnectionLost();
        public void onReadValues(int[] values);
        public void onStateChange(String state);
    }

    public LightDimmerBeacon(Context context, Listener listener) {
        mContext = context;
        mListener = listener;
        mCommandQueue = new LinkedList<byte[]>();
    }


    public void Start() {
        ScanFilter beaconFilter = new ScanFilter.Builder().setServiceUuid(
                new ParcelUuid(SERVICE_UUID)).build();
        List<ScanFilter> filters = new ArrayList<ScanFilter>();
        filters.add(beaconFilter);
        ScanSettings settings = new ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build();
        BluetoothManager manager = (BluetoothManager) mContext.getSystemService(Context.BLUETOOTH_SERVICE);
        mBluetoothScanner = manager.getAdapter().getBluetoothLeScanner();
        mBluetoothScanner.startScan(filters, settings, new MyScanCallback());
        mListener.onStateChange("Starting scan");
    }

    private class MyScanCallback extends ScanCallback {
        @Override
        public void onScanResult(int callbackType, ScanResult result) {
            BluetoothDevice device = result.getDevice();
            mListener.onStateChange("Got device " + (device != null ? device.getName() : "null"));
            if (device == null) {
                mListener.onConnectionLost();
                return;
            }
            device.connectGatt(mContext, false, new MyGattCallback());
        }
    }

    private class MyGattCallback extends BluetoothGattCallback {
        @Override
        public void onConnectionStateChange(BluetoothGatt gatt, int status, int newState) {
            mListener.onStateChange("OnStateChange " + status + ", newState: " + newState);
            if (status != BluetoothGatt.GATT_SUCCESS || newState != BluetoothGatt.STATE_CONNECTED) {
                mGatt = null;
                mListener.onConnectionLost();
                return;
            }
            gatt.discoverServices();
        }

        @Override
        public void onServicesDiscovered(BluetoothGatt gatt, int status) {
            mGatt = gatt;
            mListener.onStateChange(
                    "OnSvcDiscovererd " + status +
                    ". Num svcs: " + gatt.getServices().size());
            mCharacteristic = gatt.getService(SERVICE_UUID).getCharacteristic(CHARACTERISTC_UUID);
            gatt.setCharacteristicNotification(mCharacteristic, true);
            mListener.onDeviceConnected(gatt.getDevice().getName());
        }

        @Override
        public void onCharacteristicWrite(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic, int status) {
            Log.d("light", "Wrote " + Integer.toHexString(characteristic.getValue()[0]) + ". Status: " + status);
            mListener.onStateChange("Wrote " + Integer.toHexString(characteristic.getValue()[0]) + ". Status: " + status);
            synchronized (mCommandQueue) {
                mCommandQueue.removeFirst();
                DrainCommandQueue();
            }
        }

        @Override
        public void onCharacteristicChanged(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic) {
            String status = characteristic.getStringValue(0);
            mListener.onStateChange("RX " + status);
            int numSetpoints = status.length() / 2;
            int values[] = new int[numSetpoints];
            for (int i = 0; i < numSetpoints; ++i) {
                values[i] = Integer.parseInt(status.substring(i * 2, i * 2 + 2), 16);
            }
            mListener.onReadValues(values);
        }

    };

    public void SetBrightness(int channel, int brightness) {
        assert(channel >= 0 && channel <= 3);
        assert(brightness >= 0 && brightness <= 31);
        byte word = (byte) (channel << 5 | brightness);
        SendControlWord(word);
    }

    public void SetSmoothing(int channel, int brightness) {
        assert(channel >= 0 && channel <= 3);
        assert(brightness >= 0 && brightness <= 31);
        byte word = (byte) (channel << 5 | brightness | 0x80);
        SendControlWord(word);
    }

    private void DrainCommandQueue() {
        synchronized (mCommandQueue) {
            if (mCommandQueue.isEmpty())
                return;
            byte[] cmd = mCommandQueue.getFirst();
            try {
                Log.d("light", "Writing " + Integer.toHexString(cmd[0]));
                mCharacteristic.setValue(cmd);
                mGatt.writeCharacteristic(mCharacteristic);
            } catch (Exception ex) {
                mListener.onConnectionLost();
                Start();
                ex.printStackTrace();
            }
        }
    }

    private void SendControlWord(byte word) {
        byte[] cmd = new byte[]{word, word};
        Log.d("light", "Req " + Integer.toHexString(word));
        synchronized (mCommandQueue) {
            mCommandQueue.add(cmd);
            if (mCommandQueue.size() == 1) {
                DrainCommandQueue();
            }
        }
    }
}
