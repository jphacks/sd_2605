import type { RobotMode } from "../types";

export default function Robot({ mode = "idle", small = false }: { mode?: RobotMode; small?: boolean }) {
  return (
    <div className={`robot-stage ${small ? "robot-stage--small" : ""}`}>
      <div className={`robot robot--${mode}`} aria-label={`ロボットの状態: ${mode}`} role="img">
        <div className="robot-antenna"><i /></div>
        <div className="robot-head">
          <div className="robot-ear robot-ear--left" />
          <div className="robot-ear robot-ear--right" />
          <div className="robot-face">
            <div className="robot-eyes"><i /><i /></div>
            <div className="robot-mouth" />
          </div>
        </div>
        <div className="robot-neck" />
        <div className="robot-body">
          <div className="robot-badge"><span /></div>
          <div className="robot-arm robot-arm--left" />
          <div className="robot-arm robot-arm--right" />
        </div>
        <div className="robot-feet"><i /><i /></div>
      </div>
      <div className="robot-shadow" />
    </div>
  );
}
