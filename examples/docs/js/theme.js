(function () {
  "use strict";

  var STORAGE_KEY = "teapot-docs-theme";

  function allowlisted(value) {
    return value === "dark" || value === "light" ? value : "dark";
  }

  function readStored() {
    try {
      return allowlisted(window.localStorage.getItem(STORAGE_KEY));
    } catch (e) {
      return "dark";
    }
  }

  function applyTheme(theme) {
    var next = allowlisted(theme);
    /* setAttribute / dataset only — never interpolate storage into HTML */
    if (next === "dark") {
      document.documentElement.removeAttribute("data-theme");
    } else {
      document.documentElement.setAttribute("data-theme", "light");
    }
    try {
      window.localStorage.setItem(STORAGE_KEY, next);
    } catch (e) {
      /* ignore quota / private mode */
    }
    syncToggle(next);
  }

  function syncToggle(theme) {
    var buttons = document.querySelectorAll("[data-theme-toggle]");
    var label = theme === "light" ? "Dark" : "Light";
    var pressed = theme === "light" ? "true" : "false";
    for (var i = 0; i < buttons.length; i++) {
      buttons[i].setAttribute("aria-pressed", pressed);
      buttons[i].textContent = label;
      buttons[i].setAttribute(
        "aria-label",
        theme === "light" ? "Switch to dark theme" : "Switch to light theme"
      );
    }
  }

  function currentTheme() {
    var attr = document.documentElement.getAttribute("data-theme");
    return allowlisted(attr === "light" ? "light" : "dark");
  }

  function onToggleClick(ev) {
    ev.preventDefault();
    applyTheme(currentTheme() === "light" ? "dark" : "light");
  }

  function bind() {
    var buttons = document.querySelectorAll("[data-theme-toggle]");
    for (var i = 0; i < buttons.length; i++) {
      buttons[i].addEventListener("click", onToggleClick);
    }
    syncToggle(currentTheme());
  }

  /* Expose for early head boot if needed; head script applies storage first. */
  window.teapotDocsTheme = {
    apply: applyTheme,
    read: readStored,
    allowlisted: allowlisted,
  };

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", bind);
  } else {
    bind();
  }

  /* HX-Trigger may dispatch teapot:flash on #status-line / toys */
  document.body.addEventListener("teapot:flash", function (ev) {
    var el = ev.target;
    if (!el || !el.classList) return;
    el.classList.add("flash-accent");
    window.setTimeout(function () {
      el.classList.remove("flash-accent");
    }, 450);
  });
})();
