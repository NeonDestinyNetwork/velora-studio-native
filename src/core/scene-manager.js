/**
 * Velora Studio Native — Scene & Source Manager
 * Manages scene collections, source hierarchies, and layer transformations
 */

export class SceneManager {
  constructor() {
    this.scenes = [
      {
        id: 'scene-live',
        name: 'Live Broadcast (Gaming/Screen)',
        sources: [
          {
            id: 'src-display',
            name: 'Primary Display Capture',
            type: 'display',
            visible: true,
            locked: true,
            x: 0,
            y: 0,
            width: 1920,
            height: 1080
          },
          {
            id: 'src-webcam',
            name: 'Streamer Facecam (1080p)',
            type: 'camera',
            visible: true,
            locked: false,
            x: 1420,
            y: 740,
            width: 460,
            height: 300
          },
          {
            id: 'src-overlay',
            name: 'Velora Gold Stream Overlay',
            type: 'text',
            text: 'VELORA NETWORK • 60 FPS • LIVE',
            color: '#D4AF37',
            fontSize: 22,
            fontWeight: '800',
            visible: true,
            locked: false,
            x: 40,
            y: 40,
            width: 400,
            height: 40
          }
        ]
      },
      {
        id: 'scene-chatting',
        name: 'Just Chatting (Fullcam & Chat)',
        sources: [
          {
            id: 'src-chatting-cam',
            name: 'Main 4K Studio Cam',
            type: 'camera',
            visible: true,
            locked: true,
            x: 0,
            y: 0,
            width: 1920,
            height: 1080
          },
          {
            id: 'src-chat-box',
            name: 'Velora Live Chat Box',
            type: 'text',
            text: '💬 Live Chat Connected',
            color: '#00E5FF',
            fontSize: 28,
            visible: true,
            locked: false,
            x: 100,
            y: 200,
            width: 500,
            height: 600
          }
        ]
      },
      {
        id: 'scene-starting',
        name: 'Starting Soon (Countdown & Music)',
        sources: [
          {
            id: 'src-bg-starting',
            name: 'Cosmic Obsidian Background',
            type: 'color',
            color: '#040817',
            visible: true,
            locked: true,
            x: 0,
            y: 0,
            width: 1920,
            height: 1080
          },
          {
            id: 'src-text-starting',
            name: 'Starting Soon Text',
            type: 'text',
            text: 'VELORA STREAM STARTING SOON...',
            color: '#D4AF37',
            fontSize: 48,
            fontWeight: '900',
            visible: true,
            locked: false,
            x: 520,
            y: 500,
            width: 900,
            height: 80
          }
        ]
      }
    ];

    this.activeSceneId = 'scene-live';
    this.selectedSourceId = 'src-webcam';
    this.onSceneChanged = null;
  }

  getActiveScene() {
    return this.scenes.find(s => s.id === this.activeSceneId) || this.scenes[0];
  }

  switchScene(sceneId) {
    const target = this.scenes.find(s => s.id === sceneId);
    if (target) {
      this.activeSceneId = sceneId;
      this.selectedSourceId = target.sources[0]?.id || null;
      if (this.onSceneChanged) {
        this.onSceneChanged(target);
      }
    }
  }

  addScene(name) {
    const newScene = {
      id: `scene-${Date.now()}`,
      name: name || `Scene ${this.scenes.length + 1}`,
      sources: []
    };
    this.scenes.push(newScene);
    this.switchScene(newScene.id);
    return newScene;
  }

  addSourceToActive(sourceData) {
    const scene = this.getActiveScene();
    const newSource = {
      id: `src-${Date.now()}`,
      name: sourceData.name || 'New Source',
      type: sourceData.type || 'display',
      visible: true,
      locked: false,
      x: sourceData.x || 100,
      y: sourceData.y || 100,
      width: sourceData.width || 640,
      height: sourceData.height || 360,
      ...sourceData
    };
    scene.sources.unshift(newSource);
    this.selectedSourceId = newSource.id;
    return newSource;
  }

  toggleSourceVisibility(sourceId) {
    const scene = this.getActiveScene();
    const src = scene.sources.find(s => s.id === sourceId);
    if (src) {
      src.visible = !src.visible;
      return src.visible;
    }
    return false;
  }

  toggleSourceLock(sourceId) {
    const scene = this.getActiveScene();
    const src = scene.sources.find(s => s.id === sourceId);
    if (src) {
      src.locked = !src.locked;
      return src.locked;
    }
    return false;
  }

  deleteSource(sourceId) {
    const scene = this.getActiveScene();
    const index = scene.sources.findIndex(s => s.id === sourceId);
    if (index !== -1) {
      scene.sources.splice(index, 1);
      this.selectedSourceId = scene.sources[0]?.id || null;
    }
  }
}
