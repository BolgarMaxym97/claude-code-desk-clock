import { Composition, Still } from 'remotion';
import { Cover } from './Cover';
import { FPS, Promo, S } from './Promo';

export const Root = () => (
  <>
    <Composition id="Promo" component={Promo} durationInFrames={S.end} fps={FPS} width={1920} height={1080} />
    <Still id="Cover" component={Cover} width={1280} height={640} />
  </>
);
