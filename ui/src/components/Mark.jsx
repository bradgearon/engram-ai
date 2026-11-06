export default function Mark({ small = false }) {
  return (
    <span className={small ? "mark small" : "mark"} aria-hidden="true">
      <svg viewBox="0 0 40 40">
        <path d="M9 14l6 15 5-12 5 12 6-15M20 8v4" />
      </svg>
    </span>
  );
}
