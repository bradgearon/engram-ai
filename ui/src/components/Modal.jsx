import { useEffect, useRef } from "react";
import { X } from "lucide-react";
export default function Modal({ title, children, onClose, wide = false }) {
  const ref = useRef();
  useEffect(() => {
    ref.current.showModal();
  }, []);
  return (
    <dialog
      ref={ref}
      className={wide ? "modal wide" : "modal"}
      onCancel={onClose}
      onClick={(e) => {
        if (e.target === ref.current) onClose();
      }}
    >
      <header>
        <h2>{title}</h2>
        <button className="icon" aria-label="Close dialog" onClick={onClose}>
          <X size={19} />
        </button>
      </header>
      {children}
    </dialog>
  );
}
