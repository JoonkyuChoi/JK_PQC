/*----------------------------------------------------------------------------+-
root_page.h
-+----------------------------------------------------------------------------+-
Description : jk-pqc-server GET / 브라우저 HTML 페이지
Copyright   : 2026~ by Joonkyu Choi, All rights reserved.
-+----------------------------------------------------------------------------*/
#pragma once

// jk-pqc-server 전용 GET / 브라우저 응답 HTML (text/html, 내용은 필요 시 직접 수정)
inline const char D_ROOT_PAGE_HTML[] = R"RAW(
<!DOCTYPE html>
<html lang="ko">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>JK-PQC Server</title>
  <style>
    @import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Orbitron:wght@400;700;900&family=Rajdhani:wght@300;400;600&display=swap');

    :root {
      --c-bg:     #03070f;
      --c-border: rgba(0,210,255,0.18);
      --c-glow:   #00d2ff;
      --c-glow2:  #0066ff;
      --c-amber:  #ffaa00;
      --c-green:  #00ff88;
      --c-text:   #b8d8f0;
      --c-dim:    rgba(100,170,220,0.45);
      --c-head:   #e8f4ff;
      --font-mono: 'Share Tech Mono', monospace;
      --font-hud:  'Orbitron', sans-serif;
      --font-body: 'Rajdhani', sans-serif;
    }

    *, *::before, *::after { margin:0; padding:0; box-sizing:border-box; }

    html, body {
      width:100%; height:100%;
      background: var(--c-bg);
      color: var(--c-text);
      font-family: var(--font-body);
    }

    /* 격자 배경 */
    body::before {
      content:'';
      position:fixed; inset:0; z-index:0;
      background-image:
        linear-gradient(rgba(0,210,255,0.04) 1px, transparent 1px),
        linear-gradient(90deg, rgba(0,210,255,0.04) 1px, transparent 1px);
      background-size: 48px 48px;
      pointer-events:none;
    }
    body::after {
      content:'';
      position:fixed; inset:0; z-index:0;
      background: radial-gradient(ellipse 60% 50% at 50% 50%,
        rgba(0,80,200,0.13) 0%, transparent 70%);
      pointer-events:none;
    }

    /* 스캔라인 */
    .scanline {
      position:fixed; top:0; left:0; width:100%; height:3px;
      background: linear-gradient(90deg,
        transparent 0%, rgba(0,210,255,0.55) 50%, transparent 100%);
      animation: scan 7s linear infinite;
      z-index:10; pointer-events:none; opacity:0.45;
    }
    @keyframes scan { 0%{top:-4px} 100%{top:100vh} }

    /* 중앙 정렬 래퍼 */
    .wrapper {
      position:relative; z-index:1;
      min-height:100vh;
      display:flex;
      align-items:center;
      justify-content:center;
      padding: 2rem;
    }

    /* 카드 */
    .card {
      width: 540px;
      border: 1px solid var(--c-border);
      border-radius: 8px;
      background: rgba(6,13,26,0.88);
      backdrop-filter: blur(10px);
      box-shadow:
        0 0 48px rgba(0,130,255,0.1),
        inset 0 0 32px rgba(0,60,160,0.06);
      overflow: hidden;
      animation: fadeUp 0.65s ease both;
    }
    @keyframes fadeUp {
      from { opacity:0; transform:translateY(16px); }
      to   { opacity:1; transform:none; }
    }

    /* 카드 상단 강조선 */
    .card-top-bar {
      height: 2px;
      background: linear-gradient(90deg,
        transparent 0%, var(--c-glow2) 30%, var(--c-glow) 70%, transparent 100%);
      box-shadow: 0 0 12px rgba(0,210,255,0.4);
    }

    /* 히어로 */
    .hero {
      text-align:center;
      padding: 2.4rem 2rem 1.8rem;
      position:relative;
    }
    .hero::after {
      content:'';
      position:absolute; bottom:0; left:50%;
      transform:translateX(-50%);
      width:60px; height:1px;
      background: linear-gradient(90deg, transparent, var(--c-glow), transparent);
    }

    .eyebrow {
      font-family: var(--font-mono);
      font-size: 0.6rem;
      letter-spacing: 0.26em;
      color: var(--c-dim);
      text-transform: uppercase;
      margin-bottom: 1.1rem;
    }

    .title {
      font-family: var(--font-hud);
      font-size: 2rem;
      font-weight: 900;
      letter-spacing: 0.1em;
      color: var(--c-head);
      text-shadow: 0 0 28px rgba(0,140,255,0.35);
      margin-bottom: 0.55rem;
      line-height: 1.1;
    }
    .title span { color:var(--c-glow); text-shadow:0 0 18px rgba(0,210,255,0.65); }

    .subtitle {
      font-family: var(--font-body);
      font-size: 0.88rem;
      font-weight: 300;
      letter-spacing: 0.14em;
      color: var(--c-dim);
    }

    /* 알고리즘 그리드 */
    .algo-grid {
      display:grid;
      grid-template-columns: repeat(3,1fr);
      gap: 0.7rem;
      padding: 1.8rem 1.4rem 1.4rem;
    }

    .algo-card {
      border: 1px solid var(--c-border);
      border-radius: 5px;
      padding: 0.8rem 0.6rem;
      background: rgba(0,18,48,0.45);
      text-align: center;
      position: relative;
      overflow: hidden;
      transition: border-color .3s, box-shadow .3s;
    }
    .algo-card::before {
      content:'';
      position:absolute; top:0; left:0; right:0; height:1px;
      background: linear-gradient(90deg, transparent, var(--c-glow), transparent);
      opacity:0; transition:opacity .3s;
    }
    .algo-card:hover { border-color:rgba(0,210,255,0.38); box-shadow:0 0 18px rgba(0,150,255,0.1); }
    .algo-card:hover::before { opacity:1; }

    .algo-type {
      font-family: var(--font-mono);
      font-size: 0.52rem;
      letter-spacing: 0.16em;
      color: var(--c-dim);
      text-transform: uppercase;
      margin-bottom: 0.35rem;
    }
    .algo-name {
      font-family: var(--font-hud);
      font-size: 0.75rem;
      font-weight: 700;
      letter-spacing: 0.04em;
      color: var(--c-glow);
    }
    .algo-name.amber { color:var(--c-amber); }
    .algo-name.green { color:var(--c-green); }
    .algo-std {
      font-family: var(--font-mono);
      font-size: 0.55rem;
      color: var(--c-dim);
      margin-top: 0.22rem;
    }

    /* 하단 정보 바 */
    .info-bar {
      display:grid;
      grid-template-columns: 1fr 1fr 1fr;
      border-top: 1px solid var(--c-border);
    }
    .info-cell {
      padding: 0.85rem 1rem;
      display:flex; flex-direction:column; gap:0.22rem;
      border-right: 1px solid var(--c-border);
    }
    .info-cell:last-child { border-right:none; }
    .info-label {
      font-family: var(--font-mono);
      font-size: 0.55rem;
      letter-spacing: 0.16em;
      color: var(--c-dim);
      text-transform: uppercase;
    }
    .info-val {
      font-family: var(--font-mono);
      font-size: 0.68rem;
      color: var(--c-text);
    }
    .info-val a { color:var(--c-text); text-decoration:none; }
    .info-val a:hover { color:var(--c-glow); }

    /* 상태 도트 행 */
    .status-row {
      display:flex; align-items:center; justify-content:center;
      gap: 1.8rem;
      padding: 0.75rem 1rem;
      border-top: 1px solid var(--c-border);
      background: rgba(0,10,30,0.35);
    }
    .s-dot {
      display:flex; align-items:center; gap:0.4rem;
      font-family: var(--font-mono); font-size:0.58rem;
      letter-spacing:0.1em; color:var(--c-dim);
    }
    .dot {
      width:6px; height:6px; border-radius:50%;
      animation: pulse 2.4s ease-in-out infinite;
    }
    .dot-g { background:var(--c-green); box-shadow:0 0 7px var(--c-green); }
    .dot-b { background:var(--c-glow);  box-shadow:0 0 7px var(--c-glow);  animation-delay:.5s; }
    .dot-a { background:var(--c-amber); box-shadow:0 0 7px var(--c-amber); animation-delay:1s; }
    @keyframes pulse { 0%,100%{opacity:1} 50%{opacity:.3} }
  </style>
</head>
<body>
<div class="scanline"></div>
<div class="wrapper">
  <div class="card">
    <div class="card-top-bar"></div>

    <!-- 히어로 -->
    <div class="hero">
      <div class="eyebrow">Post&#8209;Quantum Cryptography Server</div>
      <h1 class="title">JK&#8209;<span>PQC</span>&nbsp;SERVER</h1>
      <p class="subtitle">Hybrid&nbsp;mTLS &nbsp;/&nbsp; FIPS&nbsp;203/204 &nbsp;/&nbsp; CSocketPool&nbsp;IOCP</p>
    </div>

    <!-- 알고리즘 카드 -->
    <div class="algo-grid">
      <div class="algo-card">
        <div class="algo-type">SIGNATURE</div>
        <div class="algo-name">ML&#8209;DSA&#8209;65</div>
        <div class="algo-std">FIPS 204</div>
      </div>
      <div class="algo-card">
        <div class="algo-type">KEM</div>
        <div class="algo-name amber">ML&#8209;KEM&#8209;768</div>
        <div class="algo-std">FIPS 203</div>
      </div>
      <div class="algo-card">
        <div class="algo-type">KEY EXCHANGE</div>
        <div class="algo-name green">X25519MLKEM768</div>
        <div class="algo-std">Hybrid KEM</div>
      </div>
      <div class="algo-card">
        <div class="algo-type">PROTOCOL</div>
        <div class="algo-name">TLS&nbsp;1.3</div>
        <div class="algo-std">RFC 8446</div>
      </div>
      <div class="algo-card">
        <div class="algo-type">I/O MODEL</div>
        <div class="algo-name amber">IOCP&nbsp;/&nbsp;EPOLL</div>
        <div class="algo-std">CSocketPool</div>
      </div>
      <div class="algo-card">
        <div class="algo-type">HTTP PARSER</div>
        <div class="algo-name green">LLHTTP</div>
        <div class="algo-std">Node.js Core</div>
      </div>
    </div>

    <!-- 정보 바 -->
    <div class="info-bar">
      <div class="info-cell">
        <span class="info-label">DEVELOPER</span>
        <span class="info-val">Joonkyu Choi</span>
      </div>
      <div class="info-cell">
        <span class="info-label">CONTACT</span>
        <span class="info-val"><a href="mailto:osoi@naver.com">osoi@naver.com</a></span>
      </div>
      <div class="info-cell">
        <span class="info-label">ENDPOINT</span>
        <span class="info-val">:18080</span>
      </div>
    </div>

    <!-- 상태 표시 -->
    <div class="status-row">
      <div class="s-dot"><span class="dot dot-g"></span>ONLINE</div>
      <div class="s-dot"><span class="dot dot-b"></span>TLS&nbsp;1.3</div>
      <div class="s-dot"><span class="dot dot-a"></span>mTLS&nbsp;ACTIVE</div>
    </div>

  </div>
</div>
</body>
</html>
)RAW";
