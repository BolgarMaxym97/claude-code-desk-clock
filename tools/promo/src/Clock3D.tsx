// The clock, modelled from the photos: an orange plastic cube (~5 cm) with a raised front frame, a black glass
// panel and the 240×240 screen; a USB-C port at the back. The screen is a canvas redrawn every frame.
import { RoundedBox } from '@react-three/drei';
import { useMemo } from 'react';
import * as THREE from 'three';

const RES = 3;  // canvas px per device px

export function useScreen(draw: (ctx: CanvasRenderingContext2D) => void) {
  const { canvas, tex } = useMemo(() => {
    const canvas = document.createElement('canvas');
    canvas.width = canvas.height = 240 * RES;
    const tex = new THREE.CanvasTexture(canvas);
    tex.colorSpace = THREE.SRGBColorSpace;
    tex.anisotropy = 8;
    return { canvas, tex };
  }, []);
  const ctx = canvas.getContext('2d')!;
  ctx.setTransform(RES, 0, 0, RES, 0, 0);
  ctx.imageSmoothingEnabled = false;
  draw(ctx);
  tex.needsUpdate = true;
  return tex;
}

const ORANGE = '#F0A238';

export function ClockModel({ screen, glow = 1 }: { screen: THREE.Texture; glow?: number }) {
  return (
    <group>
      <RoundedBox args={[5, 5, 5]} radius={0.42} smoothness={8} castShadow receiveShadow>
        <meshPhysicalMaterial color={ORANGE} roughness={0.62} clearcoat={0.15} clearcoatRoughness={0.6} sheen={0.3} sheenColor="#FFD9A0" />
      </RoundedBox>
      {/* raised front frame */}
      <RoundedBox args={[5.12, 5.12, 0.34]} radius={0.3} smoothness={6} position={[0, 0, 2.42]} castShadow>
        <meshPhysicalMaterial color="#F4A83E" roughness={0.55} clearcoat={0.2} />
      </RoundedBox>
      {/* black glass panel */}
      <RoundedBox args={[4.05, 4.05, 0.08]} radius={0.06} smoothness={4} position={[0, 0.02, 2.6]}>
        <meshPhysicalMaterial color="#060606" roughness={0.08} metalness={0.1} clearcoat={1} clearcoatRoughness={0.03} />
      </RoundedBox>
      {/* the screen */}
      <mesh position={[0, 0.02, 2.646]}>
        <planeGeometry args={[3.62, 3.62]} />
        <meshBasicMaterial map={screen} toneMapped={false} color={new THREE.Color(glow, glow, glow)} />
      </mesh>
      {/* glass reflection layer */}
      <mesh position={[0, 0.02, 2.65]}>
        <planeGeometry args={[4.0, 4.0]} />
        <meshPhysicalMaterial transparent opacity={0.1} roughness={0.02} clearcoat={1} color="#ffffff" depthWrite={false} />
      </mesh>
      {/* USB-C at the back */}
      <RoundedBox args={[0.9, 0.32, 0.1]} radius={0.15} smoothness={4} position={[0, -1.6, -2.47]}>
        <meshStandardMaterial color="#1A1714" roughness={0.4} metalness={0.6} />
      </RoundedBox>
    </group>
  );
}

// Soft contact shadow under the cube: a blurred rounded square painted once (deterministic for any frame order).
export function SoftShadow({ y = -2.52, opacity = 0.42 }: { y?: number; opacity?: number }) {
  const tex = useMemo(() => {
    const c = document.createElement('canvas');
    c.width = c.height = 256;
    const g = c.getContext('2d')!;
    g.filter = 'blur(22px)';
    g.fillStyle = 'rgba(90,58,26,1)';
    g.beginPath();
    g.roundRect(62, 62, 132, 132, 18);
    g.fill();
    const t = new THREE.CanvasTexture(c);
    t.colorSpace = THREE.SRGBColorSpace;
    return t;
  }, []);
  return (
    <mesh position={[0.25, y, 0.25]} rotation={[-Math.PI / 2, 0, 0]}>
      <planeGeometry args={[10, 10]} />
      <meshBasicMaterial map={tex} transparent opacity={opacity} depthWrite={false} toneMapped={false} />
    </mesh>
  );
}
