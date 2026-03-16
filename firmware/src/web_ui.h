#pragma once

const char WEB_UI_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Receipt Printer</title>
<script src="https://cdn.tailwindcss.com"></script>
<style>
  .confetti { position: fixed; pointer-events: none; z-index: 50; }
  .confetti span {
    position: absolute; display: block; width: 8px; height: 8px;
    animation: fall 2s ease-out forwards;
  }
  @keyframes fall {
    0%   { transform: translateY(0) rotate(0deg); opacity: 1; }
    100% { transform: translateY(100vh) rotate(720deg); opacity: 0; }
  }
</style>
</head>
<body class="bg-gray-100 min-h-screen flex items-center justify-center p-4">
<div class="bg-white rounded-2xl shadow-lg p-6 w-full max-w-md">
  <h1 class="text-2xl font-bold text-gray-800 mb-1 text-center">Receipt Printer</h1>
  <p class="text-gray-500 text-sm text-center mb-6">Type a message and print it</p>

  <form id="printForm" class="space-y-4">
    <div>
      <textarea id="message" name="message" rows="4" maxlength="200"
        class="w-full border border-gray-300 rounded-lg p-3 text-gray-800 focus:ring-2 focus:ring-gray-400 focus:border-transparent resize-none"
        placeholder="What do you want to print?"></textarea>
      <div class="text-right text-xs text-gray-400 mt-1">
        <span id="charCount">0</span>/200
      </div>
    </div>

    <button type="submit" id="submitBtn"
      class="w-full bg-gray-800 text-white py-3 rounded-lg font-medium hover:bg-gray-700 transition-colors disabled:opacity-50 disabled:cursor-not-allowed">
      Print
    </button>
  </form>

  <div class="flex gap-2 mt-4">
    <button onclick="doAction('/feed')"
      class="flex-1 bg-gray-200 text-gray-700 py-2 rounded-lg text-sm hover:bg-gray-300 transition-colors">
      Feed
    </button>
    <button onclick="doAction('/cut')"
      class="flex-1 bg-gray-200 text-gray-700 py-2 rounded-lg text-sm hover:bg-gray-300 transition-colors">
      Cut
    </button>
  </div>

  <div id="status" class="mt-4 text-center text-sm text-gray-500 hidden"></div>
</div>

<script>
const form = document.getElementById('printForm');
const msg = document.getElementById('message');
const charCount = document.getElementById('charCount');
const statusEl = document.getElementById('status');
const submitBtn = document.getElementById('submitBtn');

msg.addEventListener('input', () => {
  charCount.textContent = msg.value.length;
});

msg.addEventListener('keydown', (e) => {
  if (e.key === 'Enter' && !e.shiftKey) {
    e.preventDefault();
    form.dispatchEvent(new Event('submit'));
  }
});

form.addEventListener('submit', async (e) => {
  e.preventDefault();
  const text = msg.value.trim();
  if (!text) return;

  submitBtn.disabled = true;
  submitBtn.textContent = 'Printing...';

  try {
    const res = await fetch('/submit', {
      method: 'POST',
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      body: 'message=' + encodeURIComponent(text)
    });
    if (res.ok) {
      showStatus('Printed!', 'text-green-600');
      confetti();
      msg.value = '';
      charCount.textContent = '0';
    } else {
      showStatus('Print failed', 'text-red-500');
    }
  } catch (err) {
    showStatus('Connection error', 'text-red-500');
  }
  submitBtn.disabled = false;
  submitBtn.textContent = 'Print';
});

async function doAction(path) {
  try {
    await fetch(path, {method: 'POST'});
    showStatus('Done', 'text-green-600');
  } catch (err) {
    showStatus('Error', 'text-red-500');
  }
}

function showStatus(text, cls) {
  statusEl.textContent = text;
  statusEl.className = 'mt-4 text-center text-sm ' + cls;
  statusEl.classList.remove('hidden');
  setTimeout(() => statusEl.classList.add('hidden'), 2000);
}

function confetti() {
  const container = document.createElement('div');
  container.className = 'confetti';
  document.body.appendChild(container);
  const colors = ['#f44336','#e91e63','#9c27b0','#3f51b5','#2196f3','#4caf50','#ff9800'];
  for (let i = 0; i < 40; i++) {
    const span = document.createElement('span');
    span.style.left = Math.random() * 100 + 'vw';
    span.style.background = colors[Math.floor(Math.random() * colors.length)];
    span.style.animationDelay = Math.random() * 0.5 + 's';
    span.style.width = (4 + Math.random() * 6) + 'px';
    span.style.height = span.style.width;
    span.style.borderRadius = Math.random() > 0.5 ? '50%' : '0';
    container.appendChild(span);
  }
  setTimeout(() => container.remove(), 2500);
}
</script>
</body>
</html>
)rawliteral";
