import { IAudio } from '../platform/interfaces/Audio';

export class AudioSystem {
  private audio: IAudio;

  constructor(audio: IAudio) {
    this.audio = audio;
  }

  playBubble(): void {
    this.audio.play('bubble');
  }

  playFeed(): void {
    this.audio.play('feed');
  }

  playInteract(): void {
    this.audio.play('interact');
  }

  stop(): void {
    this.audio.stop();
  }
}
