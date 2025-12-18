/**
 * ai3 noVNC UI Injection Script
 * Minimal branding and keybind helper - AI controls are NATIVE in i3bar
 *
 * IMPORTANT: This script is designed to be non-intrusive and not
 * interfere with noVNC's WebSocket connection or RFB handling.
 */

(function () {
    'use strict';

    // Only run on vnc.html page (not splash page)
    if (!window.location.pathname.includes('vnc.html') &&
        !window.location.pathname.endsWith('/')) {
        console.log('[ai3] Not on vnc.html, skipping injection');
        return;
    }

    // Delay initialization to let noVNC fully load first
    function init() {
        setTimeout(function () {
            try {
                injectHeader();
                injectKeybindHelper();
                console.log('[ai3] UI enhancements loaded (minimal mode - AI controls in i3bar)');
            } catch (e) {
                console.warn('[ai3] UI injection error:', e);
            }
        }, 1500);
    }

    // Inject ai3 header branding (minimal, non-intrusive)
    function injectHeader() {
        const header = document.createElement('div');
        header.className = 'ai3-header';
        header.innerHTML = '<span class="ai3-header-logo">ai3</span>' +
            '<span class="ai3-header-sep">│</span>' +
            '<span class="ai3-header-status">' +
            '<span class="ai3-status-dot" id="ai3-status-dot"></span>' +
            '<span id="ai3-status-text">Connected</span></span>'
        document.body.appendChild(header);

        // Auto-hide header on mouse inactivity
        let hideTimeout;
        const hideHeader = function () { header.classList.add('hidden'); };
        const showHeader = function () {
            header.classList.remove('hidden');
            clearTimeout(hideTimeout);
            hideTimeout = setTimeout(hideHeader, 8000);
        };

        document.addEventListener('mousemove', showHeader, { passive: true });
        showHeader();
    }

    // Inject keybind helper button and panel
    function injectKeybindHelper() {
        // Toggle button
        const toggle = document.createElement('button');
        toggle.className = 'ai3-keybind-toggle';
        toggle.setAttribute('title', 'Keyboard Shortcuts (?)');
        toggle.setAttribute('type', 'button');
        toggle.innerHTML = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">' +
            '<rect x="2" y="4" width="20" height="16" rx="2"/>' +
            '<path d="M6 8h.01M10 8h.01M14 8h.01M18 8h.01"/>' +
            '<path d="M6 12h.01M18 12h.01"/>' +
            '<path d="M8 16h8"/></svg>';
        document.body.appendChild(toggle);

        // Panel content
        const panel = document.createElement('div');
        panel.className = 'ai3-keybind-panel';
        panel.innerHTML = '<h4>Keyboard Shortcuts</h4>' +
            '<div class="ai3-keybind-section">' +
            '<div class="ai3-keybind-section-title">AI Features (or click i3bar)</div>' +
            '<div class="ai3-keybind-row"><span class="ai3-keybind-key">Ctrl+Shift+S</span><span>Toggle Suggest</span></div>' +
            '<div class="ai3-keybind-row"><span class="ai3-keybind-key">Ctrl+Shift+T</span><span>Trigger Action</span></div>' +
            '<div class="ai3-keybind-row"><span class="ai3-keybind-key">Ctrl+Shift+O</span><span>Optimize Layout</span></div>' +
            '<div class="ai3-keybind-row"><span class="ai3-keybind-key">Ctrl+Shift+I</span><span>Show Stats</span></div>' +
            '</div>' +
            '<div class="ai3-keybind-section">' +
            '<div class="ai3-keybind-section-title">Workspaces</div>' +
            '<div class="ai3-keybind-row"><span class="ai3-keybind-key">Ctrl+Shift+1-4</span><span>Switch</span></div>' +
            '<div class="ai3-keybind-row"><span class="ai3-keybind-key">Ctrl+Shift+1-4</span><span>Move Window</span></div>' +
            '</div>' +
            '<div class="ai3-keybind-section">' +
            '<div class="ai3-keybind-section-title">Essentials</div>' +
            '<div class="ai3-keybind-row"><span class="ai3-keybind-key">Ctrl+Shift+Enter</span><span>Terminal</span></div>' +
            '<div class="ai3-keybind-row"><span class="ai3-keybind-key">Ctrl+Shift+D</span><span>App Launcher</span></div>' +
            '<div class="ai3-keybind-row"><span class="ai3-keybind-key">Ctrl+Shift+Q</span><span>Close Window</span></div>' +
            '</div>';
        document.body.appendChild(panel);

        toggle.addEventListener('click', function (e) {
            e.preventDefault();
            e.stopPropagation();
            panel.classList.toggle('visible');
        });

        document.addEventListener('keydown', function (e) {
            if (e.key === 'Escape' && panel.classList.contains('visible')) {
                panel.classList.remove('visible');
            }
            if (e.key === '?' && !e.ctrlKey && !e.altKey && !e.metaKey) {
                var tag = document.activeElement.tagName.toLowerCase();
                if (tag !== 'input' && tag !== 'textarea') {
                    panel.classList.toggle('visible');
                }
            }
        });

        document.addEventListener('click', function (e) {
            if (panel.classList.contains('visible') &&
                !panel.contains(e.target) &&
                !toggle.contains(e.target)) {
                panel.classList.remove('visible');
            }
        });
    }

    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', init);
    } else {
        init();
    }
})();
