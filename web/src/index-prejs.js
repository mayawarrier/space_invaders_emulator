// no backend, so record logs client-side and ask user 
// to include them with bug reports
LOG = ''
const ogConsole = {};
['log', 'warn', 'error', 'info'].forEach(method => {
    ogConsole[method] = console[method];

    console[method] = function (...args) {
        ogConsole[method].apply(console, args);
        LOG += args.join(' ') + '\n';
    };
});

const bugReportLink = 'https://github.com/mayawarrier/space_invaders_emulator/issues/new';

const TouchType = Object.freeze({
    NONE: 0,
    TOUCH_EVENTS: 1,
    POINTER_EVENTS: 2,
    MS_POINTER_EVENTS: 3
});

var infoBarElem = document.getElementById('info-bar');
var gameContentElem = document.getElementById('game-content');

var gameCtrlsElem = document.getElementById('game-controls');
var p1p2SelectElem = document.getElementById('playerselect-controls');
var p1ButtonElem = document.getElementById('p1-button');
var p2ButtonElem = document.getElementById('p2-button');

var rightButtonElem = document.getElementById('right-button');
var leftButtonElem = document.getElementById('left-button');
var fireButtonElem = document.getElementById('fire-button');

var gameCanvasElem = document.getElementById('canvas');
gameCanvasElem.addEventListener("webglcontextlost", (e) => window.location.reload(), false);

// Emscripten Module
var Module =
{
    // Detect touch support.
    // This just checks if touch exists; the primary device may not be a touch device.
    // https://developer.mozilla.org/en-US/docs/Web/API/Pointer_events
    // https://github.com/Modernizr/Modernizr/pull/2432/files
    // https://stackoverflow.com/questions/13225662/
    // https://stackoverflow.com/questions/4817029/
    touchType: (() => {
        if (window.PointerEvent && 'maxTouchPoints' in navigator) {
            return navigator.maxTouchPoints > 0 ? TouchType.POINTER_EVENTS : TouchType.NONE;
        }
        else if (window.navigator.msPointerEnabled && 'msMaxTouchPoints' in navigator) {
            return navigator.msMaxTouchPoints > 0 ? TouchType.MS_POINTER_EVENTS : TouchType.NONE;
        }
        // Some desktop browsers (eg. Chrome) falsely report TouchEvent support, causing the touch UI to be shown.
        // In this case, fallback to mouse events (see attachTouchListeners). Note that most desktop browsers
        // have supported pointer events for several years, so this is rarely an issue.
        else if (window.TouchEvent || 'ontouchstart' in window) {
            return TouchType.TOUCH_EVENTS;
        }
        return TouchType.NONE;
    })(),

    onTouchEvent(func, e, pressed) {
        this.ccall(func, null, ['boolean'], [pressed]);
        e.preventDefault();
    },

    onPtrEvent(func, e, pressed) {
        // some browsers (eg. Android WebView) do not set pointer
        // capture by default on touch devices
        if (pressed) {
            e.currentTarget.setPointerCapture(e.pointerId);
        } else {
            e.currentTarget.releasePointerCapture(e.pointerId);
        }
        this.ccall(func, null, ['boolean'], [pressed]);
        e.preventDefault();
    },

    attachTouchListeners(button2FuncMap) {
        switch (this.touchType) {
            case TouchType.POINTER_EVENTS:
                for (const [button, func] of button2FuncMap) {
                    button.onpointerdown =   (e) => this.onPtrEvent(func, e, true);
                    button.onpointerup =     (e) => this.onPtrEvent(func, e, false);
                    button.onpointercancel = (e) => this.onPtrEvent(func, e, false);
                }
                break;

            case TouchType.MS_POINTER_EVENTS:
                for (const [button, func] of button2FuncMap) {
                    button.onmspointerdown =   (e) => this.onPtrEvent(func, e, true);
                    button.onmspointerup =     (e) => this.onPtrEvent(func, e, false);
                    button.onmspointercancel = (e) => this.onPtrEvent(func, e, false);
                }
                break;

            case TouchType.TOUCH_EVENTS:
                for (const [button, func] of button2FuncMap) {
                    button.addEventListener('touchstart',  (e) => this.onTouchEvent(func, e, true), { passive: false });
                    button.addEventListener('touchend',    (e) => this.onTouchEvent(func, e, false), { passive: false });
                    button.addEventListener('touchcancel', (e) => this.onTouchEvent(func, e, false), { passive: false });

                    // See note above about touch events on desktop browsers.
                    // Fallback to mouse events on desktop. e.preventDefault() will prevent these from firing on touch devices.
                    button.addEventListener('mousedown', (e) => this.onTouchEvent(func, e, true));
                    button.addEventListener('mouseup',   (e) => this.onTouchEvent(func, e, false));
                }
                break;

            default: break;
        }

        // stop vibration on touch events
        if (this.touchType != TouchType.TOUCH_EVENTS &&
            this.touchType != TouchType.NONE &&
            'ontouchstart' in window) {
            for (const [button,] of button2FuncMap) {
                button.addEventListener('touchstart', (e) => e.preventDefault(), { passive: false });
                button.addEventListener('touchend',   (e) => e.preventDefault(), { passive: false });
            }
        }
    },

    // ---------------- Called from C++ --------------------------

    touchTypeString() {
        switch (this.touchType) {
            case TouchType.POINTER_EVENTS:
                return stringToNewUTF8('PointerEvent');
            case TouchType.MS_POINTER_EVENTS:
                return stringToNewUTF8('MSPointerEvent');
            case TouchType.TOUCH_EVENTS:
                return stringToNewUTF8('TouchEvent');
            default:
                return stringToNewUTF8('None');
        }
    },

    showGameControls() {
        gameCtrlsElem.style.display = 'grid';
        p1p2SelectElem.style.display = 'none';
    },

    showPlayerSelectControls() {
        gameCtrlsElem.style.display = 'none';
        p1p2SelectElem.style.display = 'grid';
    },

    // ---------------- Emscripten API --------------------------

    canvas: gameCanvasElem,

    onRuntimeInitialized() {
        if (this.touchType != TouchType.NONE) {
            const button2CppFunc = [
                [rightButtonElem, 'web_touch_right'],
                [leftButtonElem, 'web_touch_left'],
                [fireButtonElem, 'web_touch_fire']
            ];
            this.attachTouchListeners(button2CppFunc);

            p1ButtonElem.onclick = e => this.ccall('web_click_1p');
            p2ButtonElem.onclick = e => this.ccall('web_click_2p');

            gameContentElem.style.display = 'flex';
            gameContentElem.style.padding = '0';
            infoBarElem.style.display = 'none';
        }
    },

    lastStatusTime: 0,

    setStatus: (text) => {
        if (text) {
            var now = Date.now();
            if (now - Module.lastStatusTime > 30) {
                Module.lastStatusTime = now;
                infoBarElem.innerHTML = text;
            }
        } else {
            // status updates complete, show info
            infoBarElem.innerHTML =
                '<span>' +
                    'Press <span class="key">Enter</span> to insert a coin, then ' +
                    '<span class="key">1</span> to start 1-player mode.<br/>' +
                '</span><span>' +
                    'Use <span class="key">\u{2190}</span> <span class="key">\u{2192}</span> to move, ' +
                    'and <span class="key">Space</span> to fire.<br/>' +
                '</span>';
        }
    },
    // --------------------------------------------------------

    criticalError: false,
    logFileUrl: null,

    onCriticalError(event) {
        this.criticalError = true;
        event.preventDefault();

        this.setStatus = (text) => {
            if (text) console.error('[post-exception status] ' + text);
        };
        if (event.error.stack) {
            console.error('Stack trace: ' + event.error.stack.toString())
        }

        gameContentElem.remove();

        infoBarElem.style.display = 'flex';
        infoBarElem.style.height = '15vh';
        infoBarElem.style.marginTop = '10vh';

        const logBlob = new Blob([LOG], { type: 'text/plain;charset=utf-8' });

        if (this.logFileUrl) {
            URL.revokeObjectURL(this.logFileUrl);
        }
        this.logFileUrl = URL.createObjectURL(logBlob);

        infoBarElem.innerHTML =
            'Something went wrong! <br/>' +
            '<span><a href="' + bugReportLink + '">Click here</a> to report this error.</span>' +
            '<span>' +
                'Please include this ' +
                '<a href="' + this.logFileUrl + '" download="spaceinvaders.log">log file</a> ' +
                'in your report.' +
            '</span>';
    },
};

Module.setStatus('Loading...');

window.addEventListener('error', (e) => Module.onCriticalError(e));

window.addEventListener('beforeunload', () => {
    if (Module.criticalError) {
        localStorage.setItem('reloadNeeded', true)
    }
});

window.addEventListener('pageshow', (event) => {
    const reloadNeeded = localStorage.getItem('reloadNeeded');
    if (reloadNeeded == 'true') {
        localStorage.removeItem('reloadNeeded')
        if (event.persisted) {
            window.location.reload();
        }
    }
});

// Disable double tap zoom on mobile devices
document.ondblclick = e => e.preventDefault();

class Starfield
{
    constructor(canvas) {
        this.canvas = canvas;
        this.canvasCtx = canvas.getContext('2d');
        this.stars = [];

        this.onCanvasResized();
        window.addEventListener("resize", () => this.onCanvasResized());
    }

    onCanvasResized() {
        this.canvasCtx.clearRect(0, 0, this.canvas.width, this.canvas.height);

        this.canvas.width = window.innerWidth;
        this.canvas.height = window.innerHeight;

        this.stars = [];
        var numStars = Math.max(Math.round(100 * this.canvas.width / 1920), 40);

        for (let i = 0; i < numStars; i++) {
            this.stars.push({
                x: Math.random() * this.canvas.width,
                y: Math.random() * this.canvas.height,
                size: Math.random() * 2 + 0.5,
                speed: 4 * (Math.random() * 0.5 + 0.1)
            });
        }
    }

    animate() {
        this.canvasCtx.clearRect(0, 0, this.canvas.width, this.canvas.height);

        for (const star of this.stars) {
            star.y += star.speed;
            if (star.y > this.canvas.height) {
                star.y = 0;
                star.x = Math.random() * this.canvas.width;
            }
            this.canvasCtx.fillStyle = "rgba(255, 255, 255, 0.8)";
            this.canvasCtx.fillRect(star.x, star.y, star.size, star.size);
        }

        setTimeout(() => this.animate(), 64);
    }
};

var starfield = new Starfield(document.getElementById('background-canvas'));
starfield.animate();
