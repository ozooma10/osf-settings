"use strict";
let count = 0;
document.getElementById("count").addEventListener("click", () => {
  document.getElementById("result").textContent = `Click count: ${++count}.`;
});
document.getElementById("range").addEventListener("input", event => {
  document.getElementById("value").value = event.target.value;
});
document.getElementById("close").addEventListener("click", () => osfui.send("close"));
// Let OSF UI handle Escape / Back so an open select is dismissed first.
