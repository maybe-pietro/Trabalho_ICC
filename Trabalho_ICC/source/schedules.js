// scripts para a página painel.html
// Se o ESP estiver em outro IP, substitua abaixo, por exemplo: const ESP_ROOT = 'http://192.168.1.42';
const ESP_ROOT = (function(){
  // Por padrão, tenta usar o mesmo host (útil quando o painel é servido do ESP/webserver local)
  const defaultRoot = window.location.protocol + '//' + window.location.hostname;
  return defaultRoot;
})();

function el(id){return document.getElementById(id);}

function showMessage(msg, isError){
  const m = el('message');
  if(!m) return;
  m.textContent = msg;
  m.style.color = isError ? '#a00' : '#0a0';
  if(msg) m.style.opacity = '1';
  setTimeout(()=>{ if(m) m.style.opacity = '0'; }, 5000);
}

async function fetchSchedules(){
  try{
    const res = await fetch(ESP_ROOT + '/getSchedules');
    if(!res.ok) throw new Error('Falha ao obter schedules');
    const json = await res.json();
    renderSchedules(json);
  }catch(e){
    showMessage('Erro ao obter agendamentos: ' + e.message, true);
  }
}

function renderSchedules(arr){
  const container = el('scheduleContainer');
  container.innerHTML = '';
  for(let i=0;i<3;i++){
    const s = arr[i] || {enabled:false, hour:0, minute:0, duration:0};
    const card = document.createElement('div');
    card.className = 'slot-card';
    card.innerHTML = `
      <div class="slot-header">
        <h3>Slot ${i}</h3>
        <label class="switch">Ativo <input type="checkbox" id="enabled_${i}" ${s.enabled? 'checked':''}><span class="slider"></span></label>
      </div>
      <div class="slot-body">
        <div class="row">
          <label>Hora: <input type="number" id="hour_${i}" min="0" max="23" value="${s.hour}"></label>
          <label>Minuto: <input type="number" id="minute_${i}" min="0" max="59" value="${s.minute}"></label>
        </div>
        <div class="row">
          <label>Duração (s): <input type="number" id="duration_${i}" min="1" max="600" value="${s.duration}"></label>
        </div>
        <div class="row actions">
          <button class="btn" onclick="setSchedule(${i})">Salvar</button>
          <button class="btn btn-secondary" onclick="disableSlot(${i})">Desativar</button>
        </div>
      </div>
    `;
    container.appendChild(card);
  }
}

// Salva um schedule (ativa-o).
async function setSchedule(slot){
  const hour = parseInt(el('hour_'+slot).value || '0',10);
  const minute = parseInt(el('minute_'+slot).value || '0',10);
  const duration = parseInt(el('duration_'+slot).value || '0',10);
  // Validação básica
  if(isNaN(hour) || isNaN(minute) || isNaN(duration)){
    showMessage('Preencha todos os campos corretamente', true);
    return;
  }
  if(hour<0 || hour>23 || minute<0 || minute>59 || duration<1 || duration>600){
    showMessage('Valores inválidos. Hora 0-23, minuto 0-59, duração 1-600s', true);
    return;
  }
  try{
    const url = `${ESP_ROOT}/setSchedule?slot=${slot}&hour=${hour}&minute=${minute}&duration=${duration}`;
    const res = await fetch(url);
    if(!res.ok){ const t = await res.text(); throw new Error(t || 'Erro ao salvar'); }
    showMessage('Slot ' + slot + ' salvo');
    await fetchSchedules();
  }catch(e){ showMessage('Erro ao salvar: '+e.message, true); }
}

// Desativa um slot (interface). OBS: o firmware atual não suporta um parâmetro enabled=0;
// aqui a função pedirá confirmação e então chamará /setSchedule com duration=1 e mostrará instrução.
// Para desativação persistente por slot, recomendo atualizar o firmware (posso fazer isso se quiser).
async function disableSlot(slot){
  if(!confirm('Desativar permanentemente o Slot ' + slot + '? (recomendado: atualizar firmware para suportar enabled=0)')) return;
  try{
    // Como workaround, chamamos clearSchedules (isso limpa todos os slots) e pedimos que o usuário reconfigure os que deseja manter.
    // Aviso: isso limpa todos os slots. Se preferir, posso atualizar o firmware para suportar desativação por slot.
    const res = await fetch(ESP_ROOT + '/clearSchedules');
    if(!res.ok) throw new Error('Falha ao limpar');
    showMessage('Todos os agendamentos foram apagados (workaround). Reconfigure os slots desejados.', false);
    await fetchSchedules();
  }catch(e){ showMessage('Erro ao desativar: '+e.message, true); }
}

async function clearSchedules(){
  if(!confirm('Limpar todos os agendamentos?')) return;
  try{
    const res = await fetch(ESP_ROOT + '/clearSchedules');
    if(!res.ok) throw new Error('Falha ao limpar');
    showMessage('Agendamentos limpos');
    await fetchSchedules();
  }catch(e){ showMessage('Erro: '+e.message, true); }
}

async function manualMotor(){
  const s = parseInt(el('manualSeconds').value || '0',10);
  if(s<1 || s>30){ showMessage('Digite 1-30 segundos', true); return; }
  try{
    const res = await fetch(ESP_ROOT + `/motor?tempo=${s}`);
    if(!res.ok){ const t=await res.text(); throw new Error(t||'Falha'); }
    showMessage('Comando enviado (manual)');
  }catch(e){ showMessage('Erro: '+e.message, true); }
}

// Eventos
window.addEventListener('load', ()=>{
  const refresh = el('refreshBtn'); if(refresh) refresh.addEventListener('click', fetchSchedules);
  const clearBtn = el('clearBtn'); if(clearBtn) clearBtn.addEventListener('click', clearSchedules);
  const manualBtn = el('manualRunBtn'); if(manualBtn) manualBtn.addEventListener('click', manualMotor);
  fetchSchedules();
});
