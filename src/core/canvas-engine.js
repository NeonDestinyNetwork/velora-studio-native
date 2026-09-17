/**
 * Velora Studio Native — Canvas Compositor Engine
 * Hardware-accelerated multi-layer scene rendering pipeline
 */

export class CanvasEngine {
  constructor(canvasElement) {
    this.canvas = canvasElement;
    this.ctx = canvasElement.getContext('2d', { alpha: false, desynchronized: true });
    this.width = 1920;
    this.height = 1080;
    this.canvas.width = this.width;
    this.canvas.height = this.height;

    this.sources = [];
    this.selectedSourceId = null;
    this.isRunning = false;
    this.fps = 60;
    this.frameCount = 0;
    this.lastFrameTime = performance.now();
    this.currentFps = 60;
  }

  start() {
    this.isRunning = true;
    this.renderLoop();
  }

  stop() {
    this.isRunning = false;
  }

  setSources(sources) {
    this.sources = sources;
  }

  selectSource(sourceId) {
    this.selectedSourceId = sourceId;
  }

  renderLoop() {
    if (!this.isRunning) return;

    const now = performance.now();
    const delta = now - this.lastFrameTime;
    this.frameCount++;
    if (delta >= 1000) {
      this.currentFps = Math.round((this.frameCount * 1000) / delta);
      this.frameCount = 0;
      this.lastFrameTime = now;
    }

    // 1. Clear background (Cosmic Obsidian)
    this.ctx.fillStyle = '#030611';
    this.ctx.fillRect(0, 0, this.width, this.height);

    // 2. Render all visible scene sources from bottom to top
    for (const source of this.sources) {
      if (!source.visible) continue;
      this.renderSource(source);
    }

    // 3. Render transform bounding box for selected source
    if (this.selectedSourceId) {
      const selected = this.sources.find(s => s.id === this.selectedSourceId);
      if (selected && selected.visible) {
        this.renderBoundingBox(selected);
      }
    }

    requestAnimationFrame(() => this.renderLoop());
  }

  renderSource(source) {
    this.ctx.save();
    this.ctx.globalAlpha = source.opacity ?? 1.0;

    const x = source.x ?? 0;
    const y = source.y ?? 0;
    const w = source.width ?? this.width;
    const h = source.height ?? this.height;

    switch (source.type) {
      case 'display':
      case 'camera':
      case 'media':
        if (source.mediaElement && source.mediaElement.readyState >= 2) {
          this.ctx.drawImage(source.mediaElement, x, y, w, h);
        } else {
          // Fallback placeholder pattern
          this.drawSourcePlaceholder(source, x, y, w, h);
        }
        break;

      case 'color':
        this.ctx.fillStyle = source.color || '#0B1026';
        this.ctx.fillRect(x, y, w, h);
        break;

      case 'image':
        if (source.imageElement && source.imageElement.complete) {
          this.ctx.drawImage(source.imageElement, x, y, w, h);
        }
        break;

      case 'text':
        this.ctx.fillStyle = source.color || '#FFFFFF';
        this.ctx.font = `${source.fontWeight || 'bold'} ${source.fontSize || 36}px sans-serif`;
        this.ctx.fillText(source.text || '', x, y + (source.fontSize || 36));
        break;

      default:
        this.drawSourcePlaceholder(source, x, y, w, h);
        break;
    }

    this.ctx.restore();
  }

  drawSourcePlaceholder(source, x, y, w, h) {
    this.ctx.fillStyle = '#0B112C';
    this.ctx.fillRect(x, y, w, h);
    this.ctx.strokeStyle = 'rgba(212, 175, 55, 0.4)';
    this.ctx.lineWidth = 2;
    this.ctx.strokeRect(x, y, w, h);

    this.ctx.fillStyle = '#D4AF37';
    this.ctx.font = 'bold 20px sans-serif';
    this.ctx.textAlign = 'center';
    this.ctx.fillText(source.name || 'Source', x + w / 2, y + h / 2);
  }

  renderBoundingBox(source) {
    const x = source.x ?? 0;
    const y = source.y ?? 0;
    const w = source.width ?? this.width;
    const h = source.height ?? this.height;

    this.ctx.save();
    this.ctx.strokeStyle = '#D4AF37';
    this.ctx.lineWidth = 2;
    this.ctx.setLineDash([6, 4]);
    this.ctx.strokeRect(x, y, w, h);

    // Transform Corner Handles
    const handleSize = 10;
    this.ctx.fillStyle = '#FFE082';
    this.ctx.setLineDash([]);
    const corners = [
      [x, y],
      [x + w, y],
      [x, y + h],
      [x + w, y + h],
      [x + w / 2, y],
      [x + w / 2, y + h],
      [x, y + h / 2],
      [x + w, y + h / 2]
    ];

    for (const [cx, cy] of corners) {
      this.ctx.fillRect(cx - handleSize / 2, cy - handleSize / 2, handleSize, handleSize);
      this.ctx.strokeRect(cx - handleSize / 2, cy - handleSize / 2, handleSize, handleSize);
    }
    this.ctx.restore();
  }

  getStream() {
    return this.canvas.captureStream(this.fps);
  }
}
