// Captura os elementos HTML
const tempoInput = document.getElementById('tempo');
const btnRodar = document.getElementById('btnRodar');
const statusParagrafo = document.getElementById('status');

// Adiciona o evento de clique ao botão
btnRodar.addEventListener('click', () => {
    // Pega o valor do campo de texto
    const tempo = tempoInput.value;
    
    // Substitua o IP_DO_SEU_ESP32 pelo endereço IP real do seu ESP32
    const ipDoEsp32 = '192.168.0.104'; 
    const urlDaAPI = `http://${ipDoEsp32}/motor?tempo=${tempo}`;

    statusParagrafo.textContent = 'Enviando comando...';

    // Envia a requisição para a API do ESP32
    fetch(urlDaAPI)
        .then(response => {
            if (response.ok) {
                return response.text();
            } else {
                throw new Error('Erro na resposta do ESP32');
            }
        })
        .then(data => {
            statusParagrafo.textContent = `Comando enviado com sucesso! ${data}`;
        })
        .catch(error => {
            statusParagrafo.textContent = `Erro: ${error.message}`;
            console.error('Erro de rede ou na API:', error);
        });
});