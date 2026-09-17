/**
 * Velora Studio Native — Multi-Platform Live Chat Aggregator
 * Unified chat stream for Velora, Twitch, Kick & YouTube
 */

export class ChatAggregator {
  constructor() {
    this.messages = [];
    this.filter = 'all'; // 'all', 'velora', 'twitch', 'kick'
    this.onNewMessage = null;
    this.simInterval = null;
  }

  start() {
    // Initial welcome messages
    this.addMessage({
      id: 'msg-1',
      platform: 'velora',
      author: 'VeloraBot',
      badge: 'SYSTEM',
      badgeType: 'velora',
      text: 'Velora Studio connected to live chat stream. Good luck with the broadcast! 🚀',
      timestamp: new Date()
    });

    // Realistic simulated multi-chat traffic during broadcast
    const sampleChatters = [
      { author: 'NeonRider', platform: 'velora', badge: 'VIP', text: 'LETS GOOO! Studio look clean af 🔥' },
      { author: 'CyberSamurai', platform: 'twitch', badge: 'SUB', text: 'The 60fps quality is insane!' },
      { author: 'GoldViper', platform: 'velora', badge: 'FOUNDER', text: 'Obsidian & Gold theme hits different 🏆' },
      { author: 'GamerX_99', platform: 'kick', badge: 'MOD', text: 'Audio levels are crisp and balanced!' },
      { author: 'StarlightStreamer', platform: 'velora', badge: 'VIP', text: 'WHIP latency is under 150ms wow' }
    ];

    let index = 0;
    this.simInterval = setInterval(() => {
      const chatter = sampleChatters[index % sampleChatters.length];
      this.addMessage({
        id: `msg-${Date.now()}`,
        platform: chatter.platform,
        author: chatter.author,
        badge: chatter.badge,
        badgeType: chatter.platform,
        text: chatter.text,
        timestamp: new Date()
      });
      index++;
    }, 7000);
  }

  stop() {
    if (this.simInterval) {
      clearInterval(this.simInterval);
      this.simInterval = null;
    }
  }

  addMessage(msg) {
    this.messages.push(msg);
    if (this.messages.length > 200) {
      this.messages.shift();
    }
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
