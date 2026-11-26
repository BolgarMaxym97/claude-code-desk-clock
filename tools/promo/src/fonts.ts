import { loadFont as inter } from '@remotion/google-fonts/Inter';
import { loadFont as mono } from '@remotion/google-fonts/JetBrainsMono';
import { loadFont as lora } from '@remotion/google-fonts/Lora';

export const SANS = inter('normal', { weights: ['500', '600', '700', '800'], subsets: ['latin', 'cyrillic'] }).fontFamily;
export const MONO = mono('normal', { weights: ['500', '600', '700', '800'], subsets: ['latin', 'cyrillic'] }).fontFamily;
export const SERIF = lora('normal', { weights: ['500', '600'], subsets: ['latin', 'cyrillic'] }).fontFamily;
export const SERIF_I = lora('italic', { weights: ['500'], subsets: ['latin', 'cyrillic'] }).fontFamily;

// Warm "paper" palette of the video (light, unlike the dark device screen)
export const C = {
  paper: '#F3EEE4', paper2: '#E9E0D0', ink: '#1F1B16', muted: '#6F665A', clay: '#D97757', orange: '#F2A33A', card: '#FFFDF8',
};
