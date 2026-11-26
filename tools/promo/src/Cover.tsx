// Repository cover / social preview (1280×640): title on the left, the 3D clock running a work screen on the right.
import { useThree } from '@react-three/fiber';
import { ThreeCanvas } from '@remotion/three';
import { AbsoluteFill } from 'remotion';
import * as THREE from 'three';
import { ClockModel, SoftShadow, useScreen } from './Clock3D';
import { C, MONO, SANS, SERIF } from './fonts';
import { Lights, PixelMark } from './Promo';
import { drawCell } from './screen';
import { work } from './shots';

function Cam() {
  const { camera } = useThree();
  camera.position.set(-2.2, 2.4, 17.5);
  camera.lookAt(new THREE.Vector3(-0.2, 0.1, 0));
  camera.updateMatrixWorld();
  return null;
}

function Clock() {
  const screen = useScreen((ctx) => drawCell(ctx, work('bash', 1.3)));
  return (
    <>
      <Cam />
      <Lights />
      <group rotation={[0, -0.42, 0]}><ClockModel screen={screen} /><SoftShadow /></group>
    </>
  );
}

export const Cover: React.FC = () => (
  <AbsoluteFill style={{ background: `linear-gradient(135deg, ${C.paper} 0%, ${C.paper2} 100%)` }}>
    <div style={{ position: 'absolute', right: 0, top: 0, width: 640, height: 640 }}>
      <ThreeCanvas width={640} height={640} camera={{ fov: 30 }} gl={{ antialias: true, alpha: true }}><Clock /></ThreeCanvas>
    </div>
    <div style={{ position: 'absolute', left: 72, top: 92, width: 640 }}>
      <div style={{ fontFamily: MONO, fontSize: 18, fontWeight: 700, letterSpacing: 3.5, color: C.clay, display: 'flex', alignItems: 'center', gap: 12 }}>
        <PixelMark size={3} />FOR CLAUDE CODE
      </div>
      <div style={{ marginTop: 18, fontFamily: SERIF, fontWeight: 600, fontSize: 76, lineHeight: 1.02, letterSpacing: -1.5, color: C.ink }}>
        Claude Code<br />Desk Clock
      </div>
      <div style={{ marginTop: 22, fontFamily: SANS, fontWeight: 500, fontSize: 25, lineHeight: 1.4, color: C.muted, width: 560 }}>
        A tiny pixel clock that shows what Claude is doing, your usage limits, and 90+ little animations in between.
      </div>
      <div style={{ marginTop: 30, display: 'flex', gap: 12 }}>
        {['macOS · Linux · Windows', 'ESP8266', 'MIT'].map((t) => (
          <div key={t} style={{ padding: '9px 16px', borderRadius: 10, background: C.card, boxShadow: '0 6px 16px rgba(70,45,20,0.10)',
            fontFamily: SANS, fontWeight: 600, fontSize: 19, color: C.ink }}>{t}</div>
        ))}
      </div>
    </div>
    <div style={{ position: 'absolute', left: 72, bottom: 34, fontFamily: SANS, fontWeight: 500, fontSize: 15, color: C.muted }}>
      Unofficial fan project · not affiliated with Anthropic
    </div>
  </AbsoluteFill>
);
