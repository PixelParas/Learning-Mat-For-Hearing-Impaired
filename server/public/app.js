'use strict';

const LABEL_TITLES = { birds: 'Birds', ball: 'Ball', cat: 'Cat', dog: 'Dog', mom: 'Mom', dad: 'Dad' };
let lastRequest = null;
let lastState = null;
let busy = false;

const $ = (id) => document.getElementById(id);
const playButtons = [...document.querySelectorAll('[data-label]')];

function selectedSpeakers() {
  const speakers = [];
  if ($('speaker-left').checked) speakers.push('left');
  if ($('speaker-right').checked) speakers.push('right');
  return speakers;
}
function setBusy(value) {
  busy = value;
  playButtons.forEach((button) => button.disabled = value);
  $('repeat-button').disabled = value || !lastRequest;
}
function setError(message) {
  const error = $('selection-error');
  error.textContent = message || '';
  error.hidden = !message;
}
function markSelected(label) {
  playButtons.forEach((button) => button.classList.toggle('selected', button.dataset.label === label));
}
async function postJson(path, body) {
  const response = await fetch(path, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body || {})
  });
  const data = await response.json().catch(() => ({}));
  if (!response.ok) throw new Error(data.error || `Request failed (${response.status})`);
  return data;
}
async function playLabel(label) {
  const speakers = selectedSpeakers();
  if (!speakers.length) {
    setError('Choose Left, Right, or both speakers first.');
    return;
  }
  setError('');
  setBusy(true);
  try {
    const data = await postJson('/api/play', { label, speakers });
    lastRequest = { label, speakers: [...speakers] };
    markSelected(label);
    updateFromState(data.state);
  } catch (error) {
    setError(`Could not send audio command: ${error.message}`);
  } finally {
    setBusy(false);
  }
}
async function repeatLast() {
  setError('');
  setBusy(true);
  try {
    if (!lastRequest) throw new Error('Play a sound once before using Repeat.');
    // Repeat uses the same API route and payload shape as a normal sound button.
    const data = await postJson('/api/play', {
      label: lastRequest.label,
      speakers: [...lastRequest.speakers],
      repeat: true
    });
    markSelected(data.command.label);
    updateFromState(data.state);
  } catch (error) {
    setError(error.message === 'nothing_to_repeat' ? 'Play a sound once before using Repeat.' : `Repeat failed: ${error.message}`);
  } finally {
    setBusy(false);
  }
}
function formatTime(value) {
  if (!value) return 'Never';
  const date = new Date(value);
  if (Number.isNaN(date.getTime())) return '—';
  return date.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });
}
function renderActivity(events) {
  const container = $('activity-list');
  if (!events || !events.length) {
    container.innerHTML = '<p class="muted empty-state">No events yet.</p>';
    return;
  }
  container.replaceChildren();
  for (const event of events.slice(0, 10)) {
    const row = document.createElement('div');
    row.className = 'activity-row';
    const time = document.createElement('span');
    time.className = 'activity-time';
    time.textContent = formatTime(event.timestamp);
    const detail = document.createElement('span');
    detail.className = 'activity-detail';
    let description = '';
    if (event.type === 'sensor') {
      description = `${event.side.toUpperCase()} sensor: ${LABEL_TITLES[event.label] || event.label}`;
    } else if (event.type === 'repeat') {
      description = `Repeated ${LABEL_TITLES[event.label] || event.label}`;
    } else {
      description = `Played ${LABEL_TITLES[event.label] || event.label}`;
    }
    detail.textContent = description;
    const meta = document.createElement('span');
    meta.className = 'activity-meta';
    if (event.type === 'sensor') {
      const tag = document.createElement('span');
      tag.className = `tag ${event.correct ? 'success' : 'incorrect'}`;
      tag.textContent = event.correct ? 'Correct' : (event.expectedLabel ? `Expected ${LABEL_TITLES[event.expectedLabel]}` : 'No target');
      meta.appendChild(tag);
    } else {
      meta.textContent = (event.speakers || []).map(s => s === 'left' ? 'Left' : 'Right').join(' + ');
    }
    row.append(time, detail, meta);
    container.appendChild(row);
  }
}
function setDevice(side, device) {
  const sideTitle = side === 'left' ? 'Left' : 'Right';
  const onlineEl = $(`${side}-online`);
  onlineEl.textContent = device.online ? 'Online' : 'Offline';
  onlineEl.classList.toggle('online', !!device.online);
  onlineEl.classList.toggle('offline', !device.online);
  $(`${side}-speaker-status`).textContent = device.online ? 'ESP32 connected to API' : 'ESP32 not seen yet';
  $(`${side}-queue`).textContent = String(device.queuedCommands ?? 0);
  $(`${side}-seen`).textContent = device.lastSeen ? formatTime(device.lastSeen) : 'Never';
}
function updateFromState(state) {
  if (!state) return;
  lastState = state;
  const serverPill = $('server-status');
  serverPill.classList.add('online');
  serverPill.classList.remove('offline');
  serverPill.querySelector('span:last-child').textContent = 'API connected';
  $('current-target').textContent = state.currentTarget ? LABEL_TITLES[state.currentTarget] : '—';
  $('target-hint').textContent = state.currentTarget ? 'Press the matching sensor on the mat.' : 'Choose a sound to begin a trial.';
  $('success-count').textContent = String(state.successCount ?? 0);
  $('attempt-count').textContent = String(state.attemptCount ?? 0);
  lastRequest = state.lastRequest ? { label: state.lastRequest.label, speakers: [...state.lastRequest.speakers] } : lastRequest;
  $('repeat-button').disabled = busy || !lastRequest;
  if (state.currentTarget) markSelected(state.currentTarget);
  if (state.lastRequest) {
    $('last-command').textContent = `Last: ${LABEL_TITLES[state.lastRequest.label]} → ${state.lastRequest.speakers.map(s => s === 'left' ? 'Left' : 'Right').join(' + ')}`;
  }
  const result = state.lastResult || {};
  const resultBox = $('result-box');
  resultBox.className = `result-box ${result.status === 'success' ? 'success' : result.status === 'incorrect' ? 'incorrect' : result.status === 'waiting' ? 'idle' : result.status === 'idle' ? 'idle' : result.status === 'no_target' ? 'incorrect' : 'idle'}`;
  $('result-icon').textContent = result.status === 'success' ? '✓' : result.status === 'incorrect' || result.status === 'no_target' ? '!' : '◷';
  $('result-title').textContent = result.status === 'success' ? 'Success!' : result.status === 'incorrect' ? 'Try again' : result.status === 'no_target' ? 'No active target' : result.status === 'waiting' ? 'Listening for the answer' : 'Waiting to start';
  $('result-message').textContent = result.message || 'The result will appear when a sensor is pressed.';
  if (state.devices) {
    setDevice('left', state.devices.left || {});
    setDevice('right', state.devices.right || {});
  }
  renderActivity(state.recentEvents || []);
}
async function refreshState() {
  try {
    const response = await fetch('/api/state', { cache: 'no-store' });
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    updateFromState(await response.json());
  } catch (error) {
    const serverPill = $('server-status');
    serverPill.classList.add('offline');
    serverPill.classList.remove('online');
    serverPill.querySelector('span:last-child').textContent = 'API unavailable';
  }
}

playButtons.forEach((button) => button.addEventListener('click', () => playLabel(button.dataset.label)));
$('repeat-button').addEventListener('click', repeatLast);
$('speaker-left').addEventListener('change', setError.bind(null, ''));
$('speaker-right').addEventListener('change', setError.bind(null, ''));
refreshState();
setInterval(refreshState, 800);
