import "./theme.css";
import "./styles.css";

import {
  FLASH_MODE,
  canStartFlash,
  isSupportedEnvironment,
  requiresEraseConfirmation,
  resolveInstallerSelection,
  validateFirmwareIndex,
} from "./flash-config.js";
import { flashDisplay } from "./flasher.js";

const elements = {
  form: document.querySelector("#installer-form"),
  boardOptions: document.querySelector("#board-options"),
  startButton: document.querySelector("#start-button"),
  selectionSummary: document.querySelector("#selection-summary"),
  supportNotice: document.querySelector("#support-notice"),
  versionLabel: document.querySelector("#version-label"),
  flashStatus: document.querySelector("#flash-status"),
  statusTitle: document.querySelector("#status-title"),
  statusPercent: document.querySelector("#status-percent"),
  statusMessage: document.querySelector("#status-message"),
  progress: document.querySelector("#flash-progress"),
  successNotice: document.querySelector("#success-notice"),
  successMessage: document.querySelector("#success-message"),
  errorNotice: document.querySelector("#error-notice"),
  errorMessage: document.querySelector("#error-message"),
  technicalLog: document.querySelector("#technical-log"),
  logCard: document.querySelector("#log-card"),
  eraseDialog: document.querySelector("#erase-dialog"),
  eraseBoardName: document.querySelector("#erase-board-name"),
  eraseConfirmation: document.querySelector("#erase-confirmation"),
  eraseButton: document.querySelector("#erase-button"),
};

const indexUrl = new URL("firmware-index.json", document.baseURI);
let firmwareIndex;
let selectedBoardId = "";
let selectedMode = "";
let busy = false;
const environmentSupported = isSupportedEnvironment(navigator.serial, window.isSecureContext);

function selectedBoard() {
  return firmwareIndex?.boards.find((board) => board.id === selectedBoardId);
}

function setBusy(nextBusy) {
  busy = nextBusy;
  document.querySelectorAll("input, button").forEach((control) => {
    if (!elements.eraseDialog.contains(control)) {
      control.disabled = nextBusy || !environmentSupported;
    }
  });
  updateSelection();
}

function updateSelection() {
  const board = selectedBoard();
  const ready = canStartFlash(selectedBoardId, selectedMode)
    && !busy
    && environmentSupported
    && firmwareIndex;
  elements.startButton.disabled = !ready;

  if (!board || !selectedMode) {
    elements.selectionSummary.textContent = "Choose a display and an action to continue.";
    elements.startButton.textContent = "Continue";
    return;
  }

  const actionLabel = selectedMode === FLASH_MODE.UPDATE ? "Update and keep data" : "Erase and reinstall";
  elements.selectionSummary.innerHTML = `<strong>${escapeHtml(board.label)}</strong><br>${actionLabel}`;
  elements.startButton.textContent = selectedMode === FLASH_MODE.UPDATE ? "Connect & update" : "Review & install";
}

function escapeHtml(value) {
  const span = document.createElement("span");
  span.textContent = value;
  return span.innerHTML;
}

function renderBoards(boards) {
  elements.boardOptions.replaceChildren();
  for (const board of boards) {
    const label = document.createElement("label");
    label.className = "option-card";

    const input = document.createElement("input");
    input.type = "radio";
    input.name = "display-board";
    input.value = board.id;
    input.checked = board.id === selectedBoardId;
    input.disabled = busy || !environmentSupported;

    const body = document.createElement("span");
    body.className = "option-body";
    const name = document.createElement("strong");
    name.textContent = board.label;
    const note = document.createElement("span");
    note.textContent = board.description;
    body.append(name, note);
    label.append(input, body);
    elements.boardOptions.append(label);
  }
}

function appendLog(message) {
  const timestamp = new Date().toLocaleTimeString([], { hour12: false });
  elements.technicalLog.textContent += `[${timestamp}] ${message}\n`;
  elements.technicalLog.scrollTop = elements.technicalLog.scrollHeight;
}

function setStatus(message) {
  elements.statusTitle.textContent = message;
}

function setProgress(ratio) {
  const percent = Math.max(0, Math.min(100, Math.round(ratio * 100)));
  elements.progress.value = percent;
  elements.progress.textContent = `${percent}%`;
  elements.statusPercent.textContent = `${percent}%`;
}

function friendlyError(error) {
  if (error?.name === "NotFoundError") {
    return "No serial port was selected.";
  }
  if (error?.name === "NetworkError") {
    return "The serial connection was interrupted. Reconnect the display and try again.";
  }
  return error instanceof Error ? error.message : String(error);
}

async function beginFlash() {
  const board = selectedBoard();
  if (!board || !canStartFlash(selectedBoardId, selectedMode) || busy) {
    return;
  }

  elements.successNotice.hidden = true;
  elements.errorNotice.hidden = true;
  elements.flashStatus.hidden = false;
  elements.logCard.open = false;
  elements.technicalLog.textContent = "";
  setProgress(0);
  setBusy(true);

  try {
    appendLog(`Release: ${firmwareIndex.version}`);
    appendLog(`Display: ${board.label} (${board.id})`);
    appendLog(`Mode: ${selectedMode}`);
    await flashDisplay({
      board,
      mode: selectedMode,
      indexUrl,
      onStatus: setStatus,
      onProgress: setProgress,
      onLog: appendLog,
    });
    elements.flashStatus.hidden = true;
    elements.successNotice.hidden = false;
    elements.successMessage.textContent = selectedMode === FLASH_MODE.INSTALL
      ? "Unplug and reconnect the USB cable to restart HOMEsmthng. Then set up Wi-Fi and pair it with Apple Home again."
      : "Unplug and reconnect the USB cable to restart HOMEsmthng with your existing settings.";
  } catch (error) {
    appendLog(`Error: ${friendlyError(error)}`);
    elements.flashStatus.hidden = true;
    elements.errorNotice.hidden = false;
    elements.errorMessage.textContent = friendlyError(error);
    elements.logCard.open = true;
  } finally {
    setBusy(false);
  }
}

elements.form.addEventListener("change", (event) => {
  const input = event.target;
  if (!(input instanceof HTMLInputElement)) {
    return;
  }
  if (input.name === "display-board") {
    selectedBoardId = input.value;
  } else if (input.name === "flash-mode") {
    selectedMode = input.value;
  }
  updateSelection();
});

elements.form.addEventListener("submit", (event) => {
  event.preventDefault();
  if (!canStartFlash(selectedBoardId, selectedMode) || busy) {
    return;
  }

  if (requiresEraseConfirmation(selectedMode)) {
    elements.eraseConfirmation.checked = false;
    elements.eraseButton.disabled = true;
    elements.eraseBoardName.textContent = selectedBoard().label;
    elements.eraseDialog.showModal();
    return;
  }
  void beginFlash();
});

elements.eraseConfirmation.addEventListener("change", () => {
  elements.eraseButton.disabled = !elements.eraseConfirmation.checked;
});

elements.eraseButton.addEventListener("click", (event) => {
  event.preventDefault();
  if (!elements.eraseConfirmation.checked) {
    return;
  }
  elements.eraseDialog.close();
  void beginFlash();
});

async function initialize() {
  if (!environmentSupported) {
    elements.supportNotice.hidden = false;
    elements.form.querySelectorAll("input, button").forEach((control) => {
      control.disabled = true;
    });
  }

  try {
    const response = await fetch(indexUrl, { cache: "no-store" });
    if (!response.ok) {
      throw new Error(`Could not load the latest firmware (${response.status}).`);
    }
    firmwareIndex = validateFirmwareIndex(await response.json());
    elements.versionLabel.textContent = `Latest release: ${firmwareIndex.version}`;
    const selection = resolveInstallerSelection(window.location.search, firmwareIndex.boards);
    selectedBoardId = selection.boardId;
    selectedMode = selection.mode;
    renderBoards(firmwareIndex.boards);
    if (selectedMode) {
      const modeInput = elements.form.querySelector(`input[name="flash-mode"][value="${selectedMode}"]`);
      if (modeInput) {
        modeInput.checked = true;
      }
    }
    updateSelection();
  } catch (error) {
    elements.versionLabel.textContent = "Firmware is currently unavailable.";
    elements.errorNotice.hidden = false;
    elements.errorMessage.textContent = friendlyError(error);
    appendLog(`Initialization error: ${friendlyError(error)}`);
  }
}

void initialize();
