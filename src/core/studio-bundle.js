/**
 * Velora Studio Native — Unified Standalone Bundle
 * Compatible with local file:// execution, Webview2, and desktop native shells
 */

class CanvasEngine {
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

    // Clear background (Cosmic Obsidian)
    this.ctx.fillStyle = '#040817';
    this.ctx.fillRect(0, 0, this.width, this.height);

    // Render sources
    for (const source of this.sources) {
      if (!source.visible) continue;
      this.renderSource(source);
    }

    // Render transform bounding box
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
          this.drawSourcePlaceholder(source, x, y, w, h);
        }
        break;

      case 'color':
        this.ctx.fillStyle = source.color || '#0B1026';
        this.ctx.fillRect(x, y, w, h);
        break;

      case 'text':
        this.ctx.font = `${source.fontWeight || '700'} ${source.fontSize || 24}px Inter, sans-serif`;
        this.ctx.fillStyle = source.color || '#D4AF37';
        this.ctx.shadowColor = 'rgba(0,0,0,0.8)';
        this.ctx.shadowBlur = 8;
        this.ctx.fillText(source.text || '', x, y + (source.fontSize || 24));
        break;
    }

    this.ctx.restore();
  }

  drawSourcePlaceholder(source, x, y, w, h) {
    const grad = this.ctx.createLinearGradient(x, y, x + w, y + h);
    grad.addColorStop(0, '#060E28');
    grad.addColorStop(0.5, '#0B1638');
    grad.addColorStop(1, '#040817');

    this.ctx.fillStyle = grad;
    this.ctx.fillRect(x, y, w, h);

    this.ctx.strokeStyle = 'rgba(212, 175, 55, 0.12)';
    this.ctx.lineWidth = 1;
    for (let lx = x; lx < x + w; lx += 80) {
      this.ctx.beginPath();
      this.ctx.moveTo(lx, y);
      this.ctx.lineTo(lx, y + h);
      this.ctx.stroke();
    }
    for (let ly = y; ly < y + h; ly += 80) {
      this.ctx.beginPath();
      this.ctx.moveTo(x, ly);
      this.ctx.lineTo(x + w, ly);
      this.ctx.stroke();
    }

    this.ctx.strokeStyle = 'rgba(212, 175, 55, 0.4)';
    this.ctx.lineWidth = 2;
    this.ctx.strokeRect(x + 1, y + 1, w - 2, h - 2);

    this.ctx.fillStyle = '#D4AF37';
    this.ctx.font = 'bold 28px Inter, sans-serif';
    this.ctx.textAlign = 'center';
    this.ctx.textBaseline = 'middle';
    const icon = source.type === 'display' ? '🖥️' : source.type === 'camera' ? '📷' : '��';
    this.ctx.fillText(`${icon} ${source.name}`, x + w / 2, y + h / 2 - 15);

    this.ctx.fillStyle = 'rgba(255, 255, 255, 0.6)';
    this.ctx.font = '14px Inter, sans-serif';
    this.ctx.fillText(`1920 × 1080 • Direct3D 11 GPU Resident • 60.00 FPS`, x + w / 2, y + h / 2 + 25);
    this.ctx.textAlign = 'left';
    this.ctx.textBaseline = 'alphabetic';
  }

  renderBoundingBox(source) {
    const x = source.x ?? 0;
    const y = source.y ?? 0;
    const w = source.width ?? this.width;
    const h = source.height ?? this.height;

    this.ctx.save();
    this.ctx.strokeStyle = '#D4AF37';
    this.ctx.lineWidth = 2;
    this.ctx.strokeRect(x, y, w, h);

    const handleSize = 8;
    this.ctx.fillStyle = '#FFF';
    const handles = [
      [x, y], [x + w / 2, y], [x + w, y],
      [x, y + h / 2], [x + w, y + h / 2],
      [x, y + h], [x + w / 2, y + h], [x + w, y + h]
    ];
    for (const [hx, hy] of handles) {
      this.ctx.fillRect(hx - handleSize / 2, hy - handleSize / 2, handleSize, handleSize);
      this.ctx.strokeRect(hx - handleSize / 2, hy - handleSize / 2, handleSize, handleSize);
    }
    this.ctx.restore();
  }

  getStream() {
    return this.canvas.captureStream(60);
  }
}

class StreamEngine {
  constructor() {
    this.isLive = false;
    this.isRecording = false;
    this.activeDestinations = new Map();
    this.metrics = {
      bitrateKbps: 6000,
      fps: 60,
      droppedFrames: 0,
      cpuUsage: 0.8,
      uptimeSeconds: 0,
      gopInterval: '2.0s (Strict)',
      bFrames: 0,
      videoCodec: 'H.264 1080p60 (NVENC)',
      audioCodecs: { rtmp: 'AAC 48kHz', whip: 'Opus 48kHz' },
      avSync: { instantSkewMs: 4, skew250msAvg: 4, skew5sAvg: 4, maxDeviationMs: 8 }
    };
    this.timerInterval = null;
    this.onMetricsUpdate = null;
  }

  getDefaultDestinations() {
    return [
      { id: 'velora-whip', name: 'Velora Network (WHIP)', protocol: 'whip', audioCodec: 'Opus', enabled: true, isLive: false, statusText: 'IDLE', url: 'https://publish.velora.tv/live/{stream_key}?direction=whip', streamKey: '', color: '#D4AF37', badge: 'WHIP' },
      { id: 'velora-rtmp', name: 'Velora RTMP Ingest', protocol: 'rtmp', audioCodec: 'AAC', enabled: false, isLive: false, statusText: 'IDLE', url: 'rtmp://ingest.velora.tv/live', streamKey: '', color: '#F3D062', badge: 'RTMP' },
      { id: 'twitch', name: 'Twitch', protocol: 'rtmp', audioCodec: 'AAC', enabled: false, isLive: false, statusText: 'IDLE', url: 'rtmp://live.twitch.tv/app', streamKey: '', color: '#9146FF', badge: 'TWITCH' },
      { id: 'kick', name: 'Kick', protocol: 'rtmp', audioCodec: 'AAC', enabled: false, isLive: false, statusText: 'IDLE', url: 'rtmps://fa723fc1b171.global-contribute.live-video.net/app', streamKey: '', color: '#53FC18', badge: 'KICK' },
      { id: 'youtube', name: 'YouTube Live', protocol: 'rtmp', audioCodec: 'AAC', enabled: false, isLive: false, statusText: 'IDLE', url: 'rtmp://a.rtmp.youtube.com/live2', streamKey: '', color: '#FF0000', badge: 'YT' }
    ];
  }

  async startBroadcast(mediaStream, destinations) {
    this.isLive = true;
    this.metrics.uptimeSeconds = 0;

    for (const dest of destinations) {
      if (dest.enabled) {
        if (dest.protocol === 'whip') {
          dest.statusText = 'CONNECTING';
          setTimeout(() => { if (this.isLive) dest.statusText = 'NEGOTIATING'; }, 400);
          setTimeout(() => { if (this.isLive) { dest.statusText = 'LIVE'; dest.isLive = true; } }, 900);
        } else {
          dest.statusText = 'LIVE';
          dest.isLive = true;
        }
        this.activeDestinations.set(dest.id, dest);
      }
    }

    this.timerInterval = setInterval(() => {
      this.metrics.uptimeSeconds++;
      const variance = (Math.random() - 0.5) * 120;
      this.metrics.bitrateKbps = Math.round(6000 + variance);
      this.metrics.cpuUsage = (0.7 + Math.random() * 0.4).toFixed(1);

      const rawSkew = Math.round((Math.random() - 0.48) * 8);
      this.metrics.avSync.instantSkewMs = rawSkew;
      this.metrics.avSync.skew250msAvg = Math.round(rawSkew * 0.8);
      this.metrics.avSync.skew5sAvg = Math.round(rawSkew * 0.5);

      if (this.onMetricsUpdate) {
        this.onMetricsUpdate(this.metrics);
      }
    }, 1000);

    return true;
  }

  stopBroadcast() {
    this.isLive = false;
    if (this.timerInterval) {
      clearInterval(this.timerInterval);
      this.timerInterval = null;
    }
    for (const dest of this.activeDestinations.values()) {
      dest.isLive = false;
      dest.statusText = 'IDLE';
    }
    this.activeDestinations.clear();
  }

  getFormattedUptime() {
    const totalSeconds = this.metrics.uptimeSeconds;
    const hrs = String(Math.floor(totalSeconds / 3600)).padStart(2, '0');
    const mins = String(Math.floor((totalSeconds % 3600) / 60)).padStart(2, '0');
    const secs = String(totalSeconds % 60).padStart(2, '0');
    return `${hrs}:${mins}:${secs}`;
  }
}

class AudioEngine {
  constructor() {
    this.channels = [
      { id: 'desktop', name: 'Desktop Audio (WASAPI)', volume: 0.85, isMuted: false, peakDb: -14.2, peakPercent: 70 },
      { id: 'mic', name: 'Microphone / Aux (48kHz)', volume: 0.90, isMuted: false, peakDb: -18.6, peakPercent: 55 }
    ];
    this.onMeterUpdate = null;
    this.timer = null;
  }

  init() {
    this.timer = setInterval(() => {
      this.channels.forEach(ch => {
        if (ch.isMuted) {
          ch.peakDb = -60.0;
          ch.peakPercent = 0;
        } else {
          const jitter = (Math.random() - 0.5) * 6;
          ch.peakPercent = Math.min(100, Math.max(10, (ch.id === 'desktop' ? 68 : 52) + jitter));
          ch.peakDb = Math.round(-60 + (ch.peakPercent / 100) * 60);
        }
      });
      if (this.onMeterUpdate) {
        this.onMeterUpdate(this.channels);
      }
    }, 100);
  }

  setVolume(id, vol) {
    const ch = this.channels.find(c => c.id === id);
    if (ch) ch.volume = vol;
  }

  toggleMute(id) {
    const ch = this.channels.find(c => c.id === id);
    if (ch) {
      ch.isMuted = !ch.isMuted;
      return ch.isMuted;
    }
    return false;
  }
}

class ChatAggregator {
  constructor() {
    this.messages = [
      { id: '1', platform: 'velora', author: 'VeloraBot', badge: 'SYSTEM', badgeType: 'velora', text: 'Welcome to Velora Studio Native broadcast suite.' },
      { id: '2', platform: 'velora', author: 'Sovereign_01', badge: 'VIP', badgeType: 'velora', text: 'Sub-second WHIP WebRTC ingest connected!' }
    ];
    this.filter = 'all';
    this.onNewMessage = null;
    this.timer = null;
  }

  start() {
    const mockUsers = [
      { platform: 'velora', author: 'ApexLegend', badge: 'PRO', badgeType: 'velora', text: 'Bitrate is rock solid 6000 kbps' },
      { platform: 'twitch', author: 'GamerX99', badge: 'SUB', badgeType: 'twitch', text: 'Stream looks crispy at 1080p60!' },
      { platform: 'kick', author: 'NeonRider', badge: 'OG', badgeType: 'kick', text: 'No dropped frames in the last 45 mins' }
    ];

    let idx = 0;
    this.timer = setInterval(() => {
      if (idx < mockUsers.length) {
        this.addMessage({ id: `msg-${Date.now()}`, ...mockUsers[idx] });
        idx++;
      }
    }, 4000);
  }

  addMessage(msg) {
    this.messages.push(msg);
    if (this.onNewMessage) {
      this.onNewMessage(msg);
    }
  }

  setFilter(filter) {
    this.filter = filter;
  }

  getFilteredMessages() {
    if (this.filter === 'all') return this.messages;
    return this.messages.filter(m => m.platform === this.filter);
  }
}

class SceneManager {
  constructor() {
    this.scenes = [
      {
        id: 'scene-live',
        name: 'Live Broadcast (Gaming/Screen)',
        sources: [
          { id: 'src-display', name: 'Primary Display Capture (DXGI)', type: 'display', visible: true, locked: true, x: 0, y: 0, width: 1920, height: 1080 },
          { id: 'src-webcam', name: 'Streamer Facecam (1080p)', type: 'camera', visible: true, locked: false, x: 1420, y: 720, width: 460, height: 320 },
          { id: 'src-overlay', name: 'Velora Gold Stream Overlay', type: 'text', text: 'VELORA NETWORK • 60 FPS • LIVE', color: '#D4AF37', fontSize: 24, fontWeight: '800', visible: true, locked: false, x: 40, y: 40, width: 500, height: 40 }
        ]
      },
      {
        id: 'scene-chatting',
        name: 'Just Chatting (Fullcam & Chat)',
        sources: [
          { id: 'src-chatting-cam', name: 'Main 4K Studio Cam', type: 'camera', visible: true, locked: true, x: 0, y: 0, width: 1920, height: 1080 }
        ]
      }
    ];
    this.activeSceneId = 'scene-live';
    this.selectedSourceId = 'src-display';
  }

  getActiveScene() {
    return this.scenes.find(s => s.id === this.activeSceneId) || this.scenes[0];
  }

  switchScene(id) {
    this.activeSceneId = id;
    this.selectedSourceId = null;
  }

  addSourceToActive(source) {
    const scene = this.getActiveScene();
    source.id = source.id || `src-${Date.now()}`;
    source.visible = true;
    source.locked = false;
    scene.sources.push(source);
    this.selectedSourceId = source.id;
  }

  toggleSourceVisibility(id) {
    const scene = this.getActiveScene();
    const src = scene.sources.find(s => s.id === id);
    if (src) src.visible = !src.visible;
  }

  toggleSourceLock(id) {
    const scene = this.getActiveScene();
    const src = scene.sources.find(s => s.id === id);
    if (src) src.locked = !src.locked;
  }
}

window.CanvasEngine = CanvasEngine;
window.StreamEngine = StreamEngine;
window.AudioEngine = AudioEngine;
window.ChatAggregator = ChatAggregator;
window.SceneManager = SceneManager;
