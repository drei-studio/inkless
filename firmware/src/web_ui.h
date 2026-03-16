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

  <hr class="my-6 border-gray-200">

  <h2 class="text-lg font-semibold text-gray-800 mb-1 text-center">Generate</h2>
  <p class="text-gray-500 text-sm text-center mb-4">AI-generated prints</p>

  <div class="flex flex-wrap gap-2 mb-3">
    <button onclick="generate('Write a short, warm affirmation.')" class="gen-btn flex-1 min-w-[calc(50%-0.25rem)] bg-purple-100 text-purple-700 py-2 px-3 rounded-lg text-sm font-medium hover:bg-purple-200 transition-colors">Affirmation</button>
    <button onclick="generate('Write a haiku.')" class="gen-btn flex-1 min-w-[calc(50%-0.25rem)] bg-blue-100 text-blue-700 py-2 px-3 rounded-lg text-sm font-medium hover:bg-blue-200 transition-colors">Haiku</button>
    <button onclick="generate('Share a surprising fun fact.')" class="gen-btn flex-1 min-w-[calc(50%-0.25rem)] bg-green-100 text-green-700 py-2 px-3 rounded-lg text-sm font-medium hover:bg-green-200 transition-colors">Fun fact</button>
    <button onclick="generate('Surprise me with something delightful and unexpected.')" class="gen-btn flex-1 min-w-[calc(50%-0.25rem)] bg-amber-100 text-amber-700 py-2 px-3 rounded-lg text-sm font-medium hover:bg-amber-200 transition-colors">Surprise me</button>
  </div>

  <div class="flex gap-2 mb-3">
    <input id="customPrompt" type="text" placeholder="Or type your own prompt..."
      class="flex-1 border border-gray-300 rounded-lg p-2 text-sm text-gray-800 focus:ring-2 focus:ring-purple-400 focus:border-transparent">
    <button onclick="generate(document.getElementById('customPrompt').value)" id="customGenBtn"
      class="gen-btn bg-gray-800 text-white px-4 rounded-lg text-sm font-medium hover:bg-gray-700 transition-colors">Go</button>
  </div>

  <div class="flex gap-2">
    <input id="behindBackName" type="text" placeholder="Enter a name..."
      class="flex-1 border border-gray-300 rounded-lg p-2 text-sm text-gray-800 focus:ring-2 focus:ring-pink-400 focus:border-transparent">
    <button onclick="talkWellBehindBack()" id="behindBackBtn"
      class="gen-btn bg-pink-100 text-pink-700 px-3 rounded-lg text-sm font-medium hover:bg-pink-200 transition-colors whitespace-nowrap">Talk well behind my back</button>
  </div>
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

function showStatus(text, cls, autoHide) {
  statusEl.textContent = text;
  statusEl.className = 'mt-4 text-center text-sm ' + cls;
  statusEl.classList.remove('hidden');
  if (autoHide !== false) setTimeout(() => statusEl.classList.add('hidden'), 2000);
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

let cachedApiKey = null;

async function getApiKey() {
  if (cachedApiKey) return cachedApiKey;
  const res = await fetch('/config');
  const data = await res.json();
  if (!data.api_key) throw new Error('No API key configured');
  cachedApiKey = data.api_key;
  return cachedApiKey;
}

async function generate(prompt) {
  if (!prompt || !prompt.trim()) return;

  const genBtns = document.querySelectorAll('.gen-btn');
  genBtns.forEach(b => b.disabled = true);
  showStatus('Generating...', 'text-purple-600');

  try {
    const apiKey = await getApiKey();

    const res = await fetch('https://api.anthropic.com/v1/messages', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'x-api-key': apiKey,
        'anthropic-version': '2023-06-01',
        'anthropic-dangerous-direct-browser-access': 'true'
      },
      body: JSON.stringify({
        model: 'claude-haiku-4-5-20251001',
        max_tokens: 300,
        system: 'You are a receipt printer. Generate short content suitable for a 58mm thermal receipt. Maximum 10 lines, 32 characters per line. No markdown formatting. Just the content, nothing else.',
        messages: [{ role: 'user', content: prompt.trim() }]
      })
    });

    if (!res.ok) {
      const err = await res.json().catch(() => ({}));
      throw new Error(err.error?.message || 'API error ' + res.status);
    }

    const data = await res.json();
    const text = data.content[0].text;

    const printRes = await fetch('/submit', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: 'message=' + encodeURIComponent(text)
    });

    if (printRes.ok) {
      showStatus('Printed!', 'text-green-600');
      confetti();
      document.getElementById('customPrompt').value = '';
    } else {
      showStatus('Print failed', 'text-red-500');
    }
  } catch (err) {
    showStatus(err.message || 'Generation failed', 'text-red-500');
  }

  genBtns.forEach(b => b.disabled = false);
}

document.getElementById('customPrompt').addEventListener('keydown', (e) => {
  if (e.key === 'Enter') generate(e.target.value);
});

document.getElementById('behindBackName').addEventListener('keydown', (e) => {
  if (e.key === 'Enter') talkWellBehindBack();
});

async function talkWellBehindBack() {
  const name = document.getElementById('behindBackName').value.trim();
  if (!name) return;
  document.getElementById('behindBackName').value = '';

  const genBtns = document.querySelectorAll('.gen-btn');
  genBtns.forEach(b => b.disabled = true);
  showStatus('Looking up ' + name + '...', 'text-pink-600', false);

  try {
    const apiKey = await getApiKey();

    const res = await fetch('https://api.anthropic.com/v1/messages', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'x-api-key': apiKey,
        'anthropic-version': '2023-06-01',
        'anthropic-dangerous-direct-browser-access': 'true'
      },
      body: JSON.stringify({
        model: 'claude-sonnet-4-6',
        max_tokens: 1024,
        stream: true,
        system: 'You are a receipt printer. Write a warm, specific "talking well behind your back" note about the person the user names. Use the web_search tool to find out about them and their work. Reference real things they have done or made. Address it to no one in particular, like genuine praise shared among friends. Start with their first name. Maximum 10 lines, 32 characters per line. No markdown formatting. Just the note, nothing else.',
        tools: [{ type: 'web_search_20250305', name: 'web_search', max_uses: 3 }],
        messages: [{ role: 'user', content: name }]
      })
    });

    if (!res.ok) {
      const err = await res.json().catch(() => ({}));
      throw new Error(err.error?.message || 'API error ' + res.status);
    }

    const reader = res.body.getReader();
    const decoder = new TextDecoder();
    let buffer = '';
    let fullText = '';
    let searchCount = 0;

    while (true) {
      const { done, value } = await reader.read();
      if (done) break;
      buffer += decoder.decode(value, { stream: true });

      const lines = buffer.split('\n');
      buffer = lines.pop();

      for (const line of lines) {
        if (!line.startsWith('data: ')) continue;
        const json = line.slice(6);
        if (json === '[DONE]') continue;
        let event;
        try { event = JSON.parse(json); } catch { continue; }

        if (event.type === 'content_block_start') {
          if (event.content_block?.type === 'server_tool_use') {
            searchCount++;
            showStatus('Searching the web (' + searchCount + ')...', 'text-pink-600', false);
          } else if (event.content_block?.type === 'text') {
            showStatus('Writing note...', 'text-pink-600', false);
          }
        } else if (event.type === 'content_block_delta') {
          if (event.delta?.type === 'text_delta') {
            fullText += event.delta.text;
          }
        }
      }
    }

    if (!fullText) throw new Error('No text in response');

    showStatus('Printing...', 'text-pink-600', false);
    const printRes = await fetch('/submit', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: 'message=' + encodeURIComponent(fullText)
    });

    if (printRes.ok) {
      showStatus('Printed!', 'text-green-600');
      confetti();
    } else {
      showStatus('Print failed', 'text-red-500');
    }
  } catch (err) {
    showStatus(err.message || 'Generation failed', 'text-red-500');
  }

  genBtns.forEach(b => b.disabled = false);
}
</script>
</body>
</html>
)rawliteral";
