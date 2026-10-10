'use strict';

const express = require('express');
const path = require('path');
const crypto = require('crypto');

const app = express();
const PORT = Number(process.env.PORT || 3000);
const DEVICE_API_KEY = process.env.DEVICE_API_KEY || 'local-dev-token-change-before-deployment';
const IS_PRODUCTION = process.env.NODE_ENV === 'production';

if (IS_PRODUCTION && DEVICE_API_KEY === 'local-dev-token-change-before-deployment') {
  console.error('[SECURITY] Set DEVICE_API_KEY to a long random value before production use.');
  process.exit(1);
}

app.disable('x-powered-by');
app.use(express.json({ limit: '16kb' }));
app.use(express.static(path.join(__dirname, 'public'), { extensions: ['html'] }));

const LABELS = Object.freeze({
  birds: { title: 'Birds', file: 'birds.mp3' },
  ball:  { title: 'Ball',  file: 'ball.mp3' },
  cat:   { title: 'Cat',   file: 'cat.mp3' },
  dog:   { title: 'Dog',   file: 'dog.mp3' },
  mom:   { title: 'Mom',   file: 'mom.mp3' },
  dad:   { title: 'Dad',   file: 'dad.mp3' }
});
const SIDES = ['left', 'right'];
const SENSOR_LABELS = {
  left: ['birds', 'ball', 'cat'],
  right: ['dog', 'mom', 'dad']
};
const MAX_RECENT_EVENTS = 20;
const MAX_QUEUE_LENGTH = 4;

let commandSequence = 1;
const runtime = {
  currentTarget: null,
  lastRequest: null,
  lastResult: { status: 'idle', message: 'Choose a sound to start a trial.' },
  successCount: 0,
  attemptCount: 0,
  recentEvents: [],
  devices: {
    left: { lastSeen: null, lastRaw: {}, lastCommandId: null },
    right: { lastSeen: null, lastRaw: {}, lastCommandId: null }
  },
  queues: { left: [], right: [] }
};

function safeLabel(value) {
  return typeof value === 'string' && Object.hasOwn(LABELS, value) ? value : null;
}
function safeSpeakers(value) {
  if (!Array.isArray(value)) return null;
  const sides = [...new Set(value)];
  if (sides.length < 1 || sides.some((side) => !SIDES.includes(side))) return null;
  return sides;
}
function deviceAuth(req, res, next) {
  const provided = req.get('X-Device-Key') || '';
  const expected = DEVICE_API_KEY;
  const a = Buffer.from(provided);
  const b = Buffer.from(expected);
  if (a.length !== b.length || !crypto.timingSafeEqual(a, b)) {
    return res.status(401).json({ error: 'unauthorized_device' });
  }
  next();
}
function addRecentEvent(event) {
  runtime.recentEvents.unshift({ ...event, timestamp: new Date().toISOString() });
  if (runtime.recentEvents.length > MAX_RECENT_EVENTS) runtime.recentEvents.length = MAX_RECENT_EVENTS;
}
function dispatchPlay(label, speakers, source = 'dashboard', preserveLastRequest = false) {
  const canonicalSpeakers = [...speakers];
  const commandId = commandSequence++;
  const command = {
    id: commandId,
    label,
    filename: LABELS[label].file,
    speakers: canonicalSpeakers,
    issuedAt: new Date().toISOString(),
    source
  };

  // A new instruction supersedes stale queued audio so an old sound can't play
  // on a side that is no longer selected.
  runtime.queues.left.length = 0;
  runtime.queues.right.length = 0;
  for (const side of canonicalSpeakers) {
    runtime.queues[side].push({ ...command, targetDevice: side });
    if (runtime.queues[side].length > MAX_QUEUE_LENGTH) runtime.queues[side].shift();
  }

  runtime.currentTarget = label;
  if (!preserveLastRequest) {
    runtime.lastRequest = { label, speakers: canonicalSpeakers };
  }
  runtime.lastResult = {
    status: 'waiting',
    message: `Waiting for the child to press ${LABELS[label].title}.`,
    expectedLabel: label,
    commandId
  };

  addRecentEvent({ type: source === 'repeat' ? 'repeat' : 'play', label, speakers: canonicalSpeakers, commandId });
  console.log(`[COMMAND ${commandId}] ${LABELS[label].title} -> ${canonicalSpeakers.join(', ')}`);
  return command;
}

function publicState() {
  const now = Date.now();
  const devices = {};
  for (const side of SIDES) {
    const lastSeen = runtime.devices[side].lastSeen;
    devices[side] = {
      online: !!lastSeen && now - lastSeen < 7000,
      lastSeen: lastSeen ? new Date(lastSeen).toISOString() : null,
      lastRaw: runtime.devices[side].lastRaw,
      lastCommandId: runtime.devices[side].lastCommandId,
      queuedCommands: runtime.queues[side].length,
      sensorLabels: SENSOR_LABELS[side]
    };
  }
  return {
    labels: LABELS,
    sensorLabels: SENSOR_LABELS,
    currentTarget: runtime.currentTarget,
    lastRequest: runtime.lastRequest,
    lastResult: runtime.lastResult,
    successCount: runtime.successCount,
    attemptCount: runtime.attemptCount,
    devices,
    recentEvents: runtime.recentEvents
  };
}

app.get('/health', (_req, res) => res.json({ ok: true, service: 'learning-mat-api', now: new Date().toISOString() }));
app.get('/api/state', (_req, res) => res.json(publicState()));

app.post('/api/play', (req, res) => {
  const label = safeLabel(req.body?.label);
  const speakers = safeSpeakers(req.body?.speakers);
  if (!label) return res.status(400).json({ error: 'invalid_label', allowed: Object.keys(LABELS) });
  if (!speakers) return res.status(400).json({ error: 'choose_at_least_one_valid_speaker', allowed: SIDES });
  const source = req.body?.repeat === true ? 'repeat' : 'dashboard';
  const command = dispatchPlay(label, speakers, source, false);
  res.status(202).json({ ok: true, command, state: publicState() });
});

app.get('/api/device/:side/command', deviceAuth, (req, res) => {
  const side = req.params.side;
  if (!SIDES.includes(side)) return res.status(404).json({ error: 'unknown_device' });
  runtime.devices[side].lastSeen = Date.now();
  const command = runtime.queues[side].shift() || null;
  if (command) {
    runtime.devices[side].lastCommandId = command.id;
    console.log(`[DEVICE ${side}] delivered command ${command.id} (${command.label})`);
  }
  res.set('Cache-Control', 'no-store');
  res.json({ command });
});

app.post('/api/device/:side/sensor', deviceAuth, (req, res) => {
  const side = req.params.side;
  if (!SIDES.includes(side)) return res.status(404).json({ error: 'unknown_device' });
  const label = safeLabel(req.body?.label);
  if (!label || !SENSOR_LABELS[side].includes(label)) {
    return res.status(400).json({ error: 'sensor_label_not_assigned_to_device', side, assigned: SENSOR_LABELS[side] });
  }
  const raw = Number.isFinite(Number(req.body?.raw)) ? Number(req.body.raw) : null;
  runtime.devices[side].lastSeen = Date.now();
  if (raw !== null) runtime.devices[side].lastRaw[label] = raw;

  const expected = runtime.currentTarget;
  const isCorrect = !!expected && label === expected;
  runtime.attemptCount += 1;
  const event = {
    type: 'sensor', side, label, labelTitle: LABELS[label].title,
    raw, expectedLabel: expected, correct: isCorrect
  };
  addRecentEvent(event);

  // Success is sticky for this trial: a later accidental press cannot erase it.
  if (isCorrect && runtime.lastResult.status !== 'success') {
    runtime.successCount += 1;
    runtime.lastResult = {
      status: 'success',
      message: `Correct! The child selected ${LABELS[label].title}.`,
      expectedLabel: expected,
      selectedLabel: label,
      side,
      raw,
      at: new Date().toISOString()
    };
  } else if (!isCorrect && runtime.lastResult.status !== 'success') {
    runtime.lastResult = {
      status: expected ? 'incorrect' : 'no_target',
      message: expected
        ? `The child pressed ${LABELS[label].title}; the current target is ${LABELS[expected].title}.`
        : `The child pressed ${LABELS[label].title}, but no sound target is active.`,
      expectedLabel: expected,
      selectedLabel: label,
      side,
      raw,
      at: new Date().toISOString()
    };
  }

  console.log(`[SENSOR ${side}] ${label} raw=${raw} expected=${expected || 'none'} correct=${isCorrect}`);
  res.json({ ok: true, correct: isCorrect, expectedLabel: expected, state: publicState() });
});

app.use((err, _req, res, _next) => {
  console.error('[SERVER ERROR]', err);
  res.status(500).json({ error: 'internal_server_error' });
});

app.listen(PORT, '0.0.0.0', () => {
  console.log(`Learning Mat server listening on http://0.0.0.0:${PORT}`);
  console.log('Dashboard: http://localhost:' + PORT);
  console.log('Device IDs: left, right');
  console.log(`Device token: ${DEVICE_API_KEY === 'local-dev-token-change-before-deployment' ? 'DEFAULT DEV TOKEN (change before deployment)' : 'configured'}`);
});
