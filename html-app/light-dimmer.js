'use strict';

class LightDimmer {
  constructor() {
    this.onStateChange = undefined;
  }

  get isBLESupported() {
    return navigator.bluetooth !== undefined;
  }

  get connected() {
    return this._characteristic !== undefined;
  }

  connect() {
    let options = {filters:[{services:[ LightDimmer.SERVICE_UUID ]}]};
    let self = this;
    return navigator.bluetooth.requestDevice(options)
      .then(device => {
        self._device = device;
        self._updateStatus('Got device: ' + device.name);
        return device.connectGATT();
      })
      .then(server => {
        this._server = server;
        return server.getPrimaryService(LightDimmer.SERVICE_UUID);
      })
      .then(service => service.getCharacteristic(LightDimmer.CHARACTERISTIC_UUID))
      .then(characteristic => {
        self._characteristic = characteristic;
        self._updateStatus('Connected to ' + self._device.name);
      })
      .catch(error => {
        self._updateStatus('Error: ' + error);
      });
  }

  setBrightness(channel, value) {
    return this._sendControlWord(channel, value, 0);
  }

  setVelocity(channel, value) {
    return this._sendControlWord(channel, value, 1);
  }

  _sendControlWord(channel, value, flag) {
    console.assert(typeof(channel) === "number");
    console.assert(typeof(value) === "number");
    console.assert(channel >= 0 && channel <= 3);
    console.assert(value >= 0 && value <= 1);
    let word = (channel << 5) | ~~(31 * value) | (flag ? 0x80 : 0);
    let cmd = new Uint8Array([word, word]);
    return this._characteristic.writeValue(cmd);
  }

  _updateStatus(status) {
    console.log(status);
    if (this.onStateChange !== undefined)
      this.onStateChange(status);
  }
}

LightDimmer.SERVICE_UUID = 0xffe0;
LightDimmer.CHARACTERISTIC_UUID = 0xffe1;
