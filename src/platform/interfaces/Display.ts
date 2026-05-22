export interface IDisplay {
  drawSprite(id: string, x: number, y: number, w: number, h: number, flipX?: boolean): void;
  drawText(text: string, x: number, y: number, color: number, size?: number): void;
  drawRect(x: number, y: number, w: number, h: number, color: number): void;
  drawCircle(x: number, y: number, radius: number, color: number): void;
  clear(): void;
  present(): void;
}
