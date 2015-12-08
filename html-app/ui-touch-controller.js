'use strict';

class UITouchController {
  constructor(canvasElement) {
    this.values = new Float32Array(UITouchController.CHANNELS);
    this.velocity = 0;
    this.onValueChange = undefined;
    this.onVelocityChange = undefined;
    this._animating = false;
    this._prevValues = this.values.slice();
    this._canvas = canvasElement;
    this._ctx = this._canvas.getContext('2d');
    this._canvas.addEventListener('touchstart', this.onTouchStart.bind(this), true);
    this._canvas.addEventListener('touchend', this.onTouchEnd.bind(this), true);
    this._canvas.addEventListener('touchmove', this.onTouchMove.bind(this), true);
    this.redraw();
  }

  redraw() {
    let N = UITouchController.CHANNELS;
    let W = this._canvas.clientWidth;
    let H = this._canvas.clientHeight;
    this._canvas.width = W;
    this._canvas.height = H;

    for (let i = 0; i < N; ++i) {
      let X1 = W / N * i;
      let X2 = W / N * (i + 1);
      this._ctx.fillStyle = UITouchController.COLORS_LIGHT[i]
      this._ctx.fillRect(X1, 0, X2, H);
      this._ctx.fillStyle = UITouchController.COLORS_DARK[i];
      this._ctx.fillRect(X1, H * (1 - this.values[i]), X2,  H);
    }
  }

  onTouchStart(e) {
    let N = UITouchController.CHANNELS;
    let W = this._canvas.clientWidth;
    let point = [e.targetTouches[0].pageX, e.targetTouches[0].pageY];
    this._touchStartY = point[1];
    this._touchChannel = Math.min(~~(point[0] * N / W), N - 1);
    this._valuesOnTouchStart = this.values.slice();
    this._touchStartTime = performance.now();
    this._animating = true;
    this._onAnimationFrame();
    e.preventDefault();
  }

  onTouchEnd(e) {
    this._animating = false;
    if (this._touchChannel === undefined)
      return;
    let minV = 100;
    let maxV = 2000;
    var velocity = performance.now() - this._touchStartTime;
    velocity = Math.max(Math.min(velocity, maxV), minV);
    velocity = 1 - (Math.log(velocity) - Math.log(minV)) / (Math.log(maxV) - Math.log(minV));
    if (this.onVelocityChange !== undefined)
      this.onVelocityChange(this._touchChannel, velocity);

    this._touchStartY = undefined;
    this._touchChannel = undefined;
    this._valuesOnTouchStart = undefined;
    this._touchStartTime = undefined;
    e.preventDefault();
  }

  onTouchMove(e) {
    if (this._touchChannel === undefined)
      return;
    let i = this._touchChannel;
    let H = this._canvas.clientHeight;
    var dy = this._touchStartY - e.targetTouches[0].pageY;
    var value = this._valuesOnTouchStart[i] + (dy / H);
    value = Math.min(Math.max(value, 0), 1);
    this.values[i] = value;
    e.preventDefault();
  }

  _onAnimationFrame() {
    this.redraw();
    for (let i = 0; i < UITouchController.CHANNELS; ++i) {
      if (this._prevValues[i] != this.values[i]) {
        this._prevValues[i] = this.values[i];
        if (this.onValueChange !== undefined)
          this.onValueChange(i, this.values[i]);
      }
    }

    if (this._animating)
      requestAnimationFrame(this._onAnimationFrame.bind(this));
  }
}

UITouchController.CHANNELS = 4;
UITouchController.COLORS_LIGHT = [ '#F8BBD0', '#BBDEFB', '#DCEDC8', '#FFECB3' ];
UITouchController.COLORS_DARK = [ '#E91E63', '#2196F3', '#8BC34A', '#FFC107' ];
