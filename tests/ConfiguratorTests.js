'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const root = path.resolve(__dirname, '..');
const read = file => fs.readFileSync(path.join(root, file), 'utf8');
const html = read('TrueGazeConfig.html');
const script = html.match(/<script>([\s\S]*?)<\/script>/)[1];
new vm.Script(script);
const defaultsSource = script.match(/const DEFAULTS = (\{[\s\S]*?\n        \});/)[1];
const defaults = vm.runInNewContext(`(${defaultsSource})`);
const functionSource = name => {
    const start = script.indexOf(`        function ${name}(`);
    assert(start >= 0, `Missing function ${name}`);
    const end = script.indexOf('\n        }', start);
    return script.slice(start, end + '\n        }'.length);
};
const context = vm.createContext({});
vm.runInContext(`const DEFAULTS = ${defaultsSource};
let originalText = '', parsed;
${functionSource('parseIni')}
${functionSource('serializeIni')}
${functionSource('fallbackToDefaults')}
function acceptIniText(text) { originalText = text; parsed = parseIni(text); }
fallbackToDefaults();`, context);
const flatten = map => Object.fromEntries([...map].flatMap(([s, keys]) =>
    [...keys].map(([k, v]) => [`${s}:${k}`, v])));
const ini = flatten(vm.runInContext(`parseIni(${JSON.stringify(read('skyrim\\SKSE\\Plugins\\TrueGaze.ini'))})`, context));
const fallback = flatten(vm.runInContext('parsed', context));
const normal = (value, boolean = false) => {
    const text = String(value);
    if (boolean && /^(0|1)$/.test(text)) return text === '1' ? 'true' : 'false';
    if (/^(true|false)$/i.test(text)) return text.toLowerCase();
    return text.trim() !== '' && Number.isFinite(Number(text)) ? Number(text) : text;
};
assert.deepEqual(Object.keys(fallback).sort(), Object.keys(ini).sort(), 'Schema/INI key coverage');
for (const key of Object.keys(ini)) {
    const [section, name] = key.split(':');
    const boolean = defaults[section][name][0] === 'bool';
    assert.equal(normal(fallback[key], boolean), normal(ini[key], boolean), `Offline/default drift: ${key}`);
}
assert.equal(read('skyrim\\TrueGazeConfig.html'), html, 'Packaged webpage differs');

const config = read('src\\Engine\\ConfigManager.cpp');
const header = read('src\\Engine\\ConfigManager.hpp');
const schema = Object.assign({}, ...Object.values(defaults));
for (const [, key, lo, hi] of config.matchAll(/clampReport\("(\w+)",\s*\w+,\s*([-\d.]+)f,\s*([-\d.]+)f\)/g)) {
    assert.equal(schema[key][3], Number(lo), `Slider minimum differs from runtime: ${key}`);
    assert.equal(schema[key][4], Number(hi), `Slider maximum differs from runtime: ${key}`);
}
const readEntries = [...config.matchAll(/(?:Read(?:Bool|Float)|GetPrivateProfileIntA)\("(\w+)",\s*"(\w+)",\s*(\w+),\s*p\)/g)];
for (const [, section, key, field] of readEntries) {
    assert(`${section}:${key}` in ini, `Runtime key absent from schema/INI: ${section}:${key}`);
    const match = header.match(new RegExp(`\\b${field}\\{([^}]+)\\}`));
    assert(match, `Missing compiled default: ${field}`);
    const boolean = defaults[section][key][0] === 'bool';
    assert.equal(normal(match[1].replace(/f$/, ''), boolean), normal(ini[`${section}:${key}`], boolean),
        `Compiled/shipped default drift: ${field}`);
}
for (const match of config.matchAll(/(?:Read(?:Bool|Float)|GetPrivateProfile(?:Int|String)A)\("(\w+)",\s*"(\w+)"/g)) {
    assert(`${match[1]}:${match[2]}` in ini, `Runtime key missing: ${match[1]}:${match[2]}`);
}
for (const key of Object.keys(ini)) {
    const [section, name] = key.split(':');
    assert(config.includes(`"${section}", "${name}"`), `INI key not read by runtime: ${key}`);
}

// Saving an older INI must persist new keys while preserving unrelated content.
vm.runInContext(`acceptIniText('; user comment\\r\\n[Visuals]\\r\\nbShowHcepPanel=false\\r\\ncustomKey=keep\\r\\n');
parsed.get('Visuals').set('bShowHcepPanel', 'true');
parsed.get('Visuals').set('bIncomingText', 'true');
parsed.set('Debug', new Map([['bTraceBlinks', 'true']]));
`, context);
const saved = vm.runInContext('serializeIni()', context);
assert(saved.includes('; user comment\r\n'));
assert(saved.includes('customKey=keep'));
const savedMap = flatten(vm.runInContext(`parseIni(${JSON.stringify(saved)})`, context));
assert.equal(savedMap['Visuals:bShowHcepPanel'], 'true');
assert.equal(savedMap['Visuals:bIncomingText'], 'true');
assert.equal(savedMap['Debug:bTraceBlinks'], 'true');
vm.runInContext(`acceptIniText(${JSON.stringify(saved)});`, context);
assert.equal(vm.runInContext('serializeIni()', context), saved, 'Repeated saves must not duplicate keys');

const commands = read('src\\Integrations\\ConsoleCommands.cpp');
const webPresets = vm.runInNewContext('(' + script.match(/function applyPreset\(presetName\) \{\s*const p = (\{[\s\S]*?\n            \})\[presetName\];/)[1] + ')');
const nativePresets = {};
for (const [, preset, section, key, value] of commands.matchAll(/\{"(vanilla|subtle|intense|social|developer)", "(\w+)", "(\w+)", "([^"]+)"\}/g)) {
    (nativePresets[preset] ||= {})[`${section}:${key}`] = normal(value);
}
for (const [name, values] of Object.entries(webPresets)) {
    const normalized = Object.fromEntries(Object.entries(values).map(([k, v]) => [k, normal(v)]));
    assert.deepEqual(nativePresets[name], normalized, `Console/web preset drift: ${name}`);
}
const table = commands.match(/constexpr CommandDef kCommands\[\] = \{([\s\S]*?)\n        \};/)[1];
const names = [...table.matchAll(/\{"(stg\w*)",/g)].map(m => m[1]).sort();
for (const doc of ['docs\\USER_GUIDE.md', 'docs\\DEVELOPER_GUIDE.md', 'docs\\CONSOLE_COMMANDS_CARD.md']) {
    const listed = [...read(doc).matchAll(/^\| `(stg\w*)` \|/gm)].map(m => m[1]).sort();
    assert.deepEqual(listed, names, `Console library coverage: ${doc}`);
}

const callbacks = [], handlers = {}, pupils = [], labels = [];
const drawing = new Proxy({}, {get(target, key) {
    if (key in target) return target[key];
    if (key === 'createRadialGradient') return () => ({addColorStop() {}});
    if (key === 'arc') return (x, y, radius) => { if (radius === 14) pupils.push({x, y}); };
    if (key === 'fillText') return (text, x) => labels.push({text, x});
    return () => {};
}});
const fakeCanvas = {width: 640, height: 280, getContext: () => drawing};
const fakeWrap = {
    addEventListener: (event, handler) => { handlers[event] = handler; },
    getBoundingClientRect: () => ({left: 0, top: 0, width: 320, height: 140})
};
vm.runInContext(`fallbackToDefaults();
parsed.get('Kinematics').set('fMicroJitterAmp', '0');`, context);
context.$ = id => id === 'eyeCanvas' ? fakeCanvas : fakeWrap;
context.performance = {now: () => 0};
context.requestAnimationFrame = callback => callbacks.push(callback);
vm.runInContext(`${functionSource('binocularEyeAngles')}
${functionSource('initEyeKinematicsSimulation')}
initEyeKinematicsSimulation();`, context);
assert.equal(callbacks.length, 1, 'Only one simulation animation loop');
handlers.mousemove({clientX: 160, clientY: 70});
callbacks.shift()(16);
assert.equal(pupils.length, 2, 'Render both pupils in the same frame');
assert(pupils[0].x > 160 && pupils[1].x < 480, 'Eyes converge inward on a centered finite-depth target');
assert(Math.abs((pupils[0].x - 160) - (480 - pupils[1].x)) < 1e-8, 'Centered convergence is symmetric');
assert(pupils.every(pupil => pupil.y === 140), 'Centered vertical pointer has no vertical offset');
const right = labels.find(label => label.text === 'RIGHT EYE');
const left = labels.find(label => label.text === 'LEFT EYE');
assert(right && left && right.x < left.x, 'Anatomical right appears on viewer left');
assert.equal(callbacks.length, 1, 'One scheduled next frame for the pair');
let clock = 16;
const advance = frames => {
    for (let frame = 0; frame < frames; ++frame) {
        pupils.length = 0;
        callbacks.shift()(clock += 16);
    }
    return pupils.map(pupil => ({...pupil}));
};
// Hold an off-center target while the head is still catching up.
handlers.mousemove({clientX: 290, clientY: 40});
advance(120);
const fixation = advance(1);
advance(60);
for (let eye = 0; eye < 2; ++eye) {
    assert(Math.abs(pupils[eye].x - fixation[eye].x) < 1e-8, 'Head follow-through must not drift horizontal fixation');
    assert(Math.abs(pupils[eye].y - fixation[eye].y) < 1e-8, 'Head follow-through must not drift vertical fixation');
}
vm.runInContext(`parsed.get('Kinematics').set('fMicroJitterAmp', '3');`, context);
const held = advance(1);
advance(60);
for (let eye = 0; eye < 2; ++eye) {
    assert(Math.abs(pupils[eye].x - held[eye].x) < 1e-8, 'Held pointer must stay stable even with high autonomous jitter');
    assert(Math.abs(pupils[eye].y - held[eye].y) < 1e-8, 'Held pointer must not wobble vertically');
}
handlers.mousemove({clientX: 160, clientY: 70});
advance(120);
assert(Math.abs((pupils[0].x - 160) - (480 - pupils[1].x)) < 1e-8, 'Returning to center restores symmetric convergence');
for (const target of [{x: 320, y: 140}, {x: 80, y: 80}, {x: 560, y: 210}]) {
    for (const eyeX of [160, 480]) {
        const angles = vm.runInContext(`binocularEyeAngles(${eyeX}, 140, ${target.x}, ${target.y}, 640)`, context);
        const dx = Math.tan(angles.yaw * Math.PI / 180) * 640;
        const dy = Math.tan(angles.pitch * Math.PI / 180) * Math.hypot(dx, 640);
        assert(Math.abs(eyeX + dx - target.x) < 1e-8, 'Each eye ray reaches shared target X');
        assert(Math.abs(140 + dy - target.y) < 1e-8, 'Each eye ray reaches shared target Y');
    }
}
const near = vm.runInContext('binocularEyeAngles(160, 140, 320, 140, 320).yaw', context);
const far = vm.runInContext('binocularEyeAngles(160, 140, 320, 140, 32000).yaw', context);
assert(near > far && far < 0.3, 'Convergence decreases with target depth');
console.log(`Configurator tests passed: ${Object.keys(ini).length} keys, five presets, ${names.length} commands, offline/save behavior and binocular target/convergence geometry.`);
