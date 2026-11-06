export function stamp(ms) {
  return new Date(ms || Date.now()).toLocaleTimeString([], {
    hour: "2-digit",
    minute: "2-digit",
  });
}
export function shortKey(value) {
  return value ? value.split(":")[0] : "Automatic";
}
export function label(value) {
  return String(value || "")
    .replaceAll("_", " ")
    .replaceAll(".", " · ");
}
export function errorMessage(error) {
  return error instanceof Error ? error.message : String(error);
}
export default { stamp, shortKey, label, errorMessage };
