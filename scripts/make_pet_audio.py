"""Original offline synthesized pet music and cues; stdlib only, no downloaded audio."""
from pathlib import Path
import math, struct, wave, hashlib, json

root = Path(__file__).resolve().parents[1]
out = root/'assets/pet-audio'
out.mkdir(parents=True, exist_ok=True)
rate = 16000
def tone(data, at, duration, frequency, level, bend=0):
    start, count = round(at*rate), round(duration*rate)
    for i in range(count):
        if start+i >= len(data): break
        t=i/rate
        envelope=min(1,t/.008)*min(1,(duration-t)/.025)*math.exp(-t*4)
        phase=2*math.pi*(frequency*t+bend*t*t/2)
        sample=math.sin(phase)+.18*math.sin(2*phase)+.07*math.sin(3*phase)
        data[start+i]+=level*envelope*sample

def melody(seconds, notes, level=6000):
    data=[0.0]*round(seconds*rate)
    for at,duration,freq in notes: tone(data,at,duration,freq,level)
    return [round(max(-26000,min(26000,x))) for x in data]

# Eight bars at 120 BPM. Each note ends before the loop seam.
bg=[0.0]*(rate*16)
chords=[(261.626,329.628,391.995),(220,261.626,329.628),
        (174.614,220,261.626),(195.998,246.942,293.665)]*2
mel=[523.251,659.255,783.991,659.255,587.33,523.251,440,523.251,
     523.251,698.456,659.255,523.251,587.33,493.883,391.995,0,
     659.255,783.991,1046.502,783.991,659.255,523.251,440,523.251,
     698.456,659.255,523.251,440,493.883,587.33,523.251,0]
for bar,chord in enumerate(chords):
    for beat in range(4):
        tone(bg,bar*2+beat*.5,.42,chord[beat%3],650)
for n,freq in enumerate(mel):
    if freq: tone(bg,n*.5,.40,freq,1000)
bg=[round(x) for x in bg]
cues={
    'touch': melody(.34,[(0,.22,640),(.12,.20,850)],4200),
    'meal': melody(.25,[(0,.13,523.251),(.11,.13,659.255)],4800),
    'rest': melody(.32,[(0,.15,523.251),(.14,.16,391.995)],3800),
    'done': melody(.26,[(0,.12,659.255),(.12,.13,783.991)],4500),
    'stamp': melody(.42,[(0,.13,523.251),(.12,.13,659.255),(.25,.15,1046.502)],5000),
    'win': melody(.43,[(0,.12,659.255),(.12,.12,783.991),(.24,.18,1046.502)],5200),
    'retry': melody(.22,[(0,.10,523.251),(.10,.10,440)],3300),
}
for i,f in enumerate((523.251,659.255,783.991,1046.502)):
    cues['note'+str(i)]=melody(.20,[(0,.19,f)],6000)
arrays={'music':bg,**cues}
metadata={}
chunks=['/* Original generated PCM; see scripts/make_pet_audio.py. */\n#include "pet_audio_pcm.h"\n']
for name,data in arrays.items():
    payload=struct.pack('<'+'h'*len(data),*data)
    wav=out/(name+'.wav')
    with wave.open(str(wav),'wb') as f:
        f.setnchannels(1);f.setsampwidth(2);f.setframerate(rate);f.writeframes(payload)
    metadata[name]={'samples':len(data),'peak':max(abs(x) for x in data),'wav_sha256':hashlib.sha256(wav.read_bytes()).hexdigest()}
    chunks.append(f'static const int16_t pcm_{name}[] = {{\n')
    for i in range(0,len(data),24): chunks.append('    '+','.join(map(str,data[i:i+24]))+',\n')
    chunks.append('};\n')
chunks.append('const pa_pcm_t pa_music = {pcm_music, sizeof(pcm_music)/sizeof(pcm_music[0])};\n')
chunks.append('const pa_pcm_t pa_cues[PA_CUE_COUNT] = {{0,0},\n')
for name in cues: chunks.append(f'    {{pcm_{name}, sizeof(pcm_{name})/sizeof(pcm_{name}[0])}},\n')
chunks.append('};\n')
(root/'src/pet_audio_pcm.c').write_text(''.join(chunks),encoding='utf-8')
(out/'conversion.json').write_text(json.dumps({'source':'Original synthesized instrument sounds and eight-bar melody, generated locally; no third-party recordings','rate':rate,'channels':1,'bits':16,'pcm_bytes':sum(len(x)*2 for x in arrays.values()),'files':metadata},ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print('Original PCM bytes:',sum(len(x)*2 for x in arrays.values()))
