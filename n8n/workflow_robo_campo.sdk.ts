import { workflow, node, trigger, sticky, ifElse, expr } from '@n8n/workflow-sdk';

const PG = { postgres: { id: 'vHkjYCcR97jtmaam', name: 'Postgres account' } };
const TG = { telegramApi: { id: '9QXOYJilBHrOE88q', name: 'Telegram account' } };

const receber = trigger({
  type: 'n8n-nodes-base.webhook',
  version: 2.1,
  config: {
    name: 'Telemetria do robô (ESP32)',
    position: [0, 300],
    parameters: { httpMethod: 'POST', path: 'robo-campo', responseMode: 'responseNode', options: {} }
  },
  output: [{ body: { robo_id: 'robo-campo-01', temp_ar: 33.5, umid_ar: 41, umid_solo: 22, luz: 85, distancia_cm: 120, classe: 'IRRIGAR', acao: 'Parou e abriu a valvula de irrigacao', modo: 'IRRIGANDO', sensor_ok: true, irrigando: true, evento: true } }]
});

const ler = node({
  type: 'n8n-nodes-base.set',
  version: 3.4,
  config: {
    name: 'Ler telemetria',
    position: [240, 300],
    parameters: {
      mode: 'manual',
      includeOtherFields: false,
      assignments: {
        assignments: [
          { id: 't1', name: 'robo_id', value: expr('{{ $json.body.robo_id ?? "robo-campo-01" }}'), type: 'string' },
          { id: 't2', name: 'temp_ar', value: expr('{{ $json.body.temp_ar }}'), type: 'number' },
          { id: 't3', name: 'umid_ar', value: expr('{{ $json.body.umid_ar }}'), type: 'number' },
          { id: 't4', name: 'umid_solo', value: expr('{{ $json.body.umid_solo }}'), type: 'number' },
          { id: 't5', name: 'luz', value: expr('{{ $json.body.luz }}'), type: 'number' },
          { id: 't6', name: 'distancia_cm', value: expr('{{ $json.body.distancia_cm }}'), type: 'number' },
          { id: 't7', name: 'classe', value: expr('{{ $json.body.classe ?? "NORMAL" }}'), type: 'string' },
          { id: 't8', name: 'acao', value: expr('{{ $json.body.acao ?? "" }}'), type: 'string' },
          { id: 't9', name: 'modo', value: expr('{{ $json.body.modo ?? "" }}'), type: 'string' },
          { id: 't10', name: 'sensor_ok', value: expr('{{ $json.body.sensor_ok !== false }}'), type: 'boolean' },
          { id: 't11', name: 'irrigando', value: expr('{{ $json.body.irrigando === true }}'), type: 'boolean' },
          { id: 't12', name: 'evento', value: expr('{{ $json.body.evento === true }}'), type: 'boolean' }
        ]
      }
    }
  },
  output: [{ robo_id: 'robo-campo-01', temp_ar: 33.5, umid_ar: 41, umid_solo: 22, luz: 85, distancia_cm: 120, classe: 'IRRIGAR', acao: 'Parou e abriu a valvula de irrigacao', modo: 'IRRIGANDO', sensor_ok: true, irrigando: true, evento: true }]
});

const precisaIA = ifElse({
  version: 2.2,
  config: {
    name: 'Mudou a situação? (pede IA)',
    position: [480, 300],
    parameters: {
      conditions: {
        options: { caseSensitive: true, leftValue: '', typeValidation: 'loose' },
        conditions: [
          { leftValue: expr('{{ $json.evento }}'), rightValue: true, operator: { type: 'boolean', operation: 'true', singleValue: true } },
          { leftValue: expr('{{ $json.classe }}'), rightValue: 'NORMAL', operator: { type: 'string', operation: 'notEquals' } }
        ],
        combinator: 'and'
      }
    }
  }
});

const ia = node({
  type: '@n8n/n8n-nodes-langchain.openAi',
  version: 2.3,
  config: {
    name: 'IA: recomendação agronômica (GPT-4o mini)',
    position: [720, 180],
    onError: 'continueRegularOutput',
    parameters: {
      resource: 'text',
      operation: 'response',
      modelId: { __rl: true, mode: 'id', value: 'gpt-4o-mini' },
      responses: {
        values: [{
          type: 'text',
          role: 'user',
          content: expr('Leituras do robô de campo agora:
- Temperatura do ar: {{ $json.temp_ar }} °C
- Umidade do ar: {{ $json.umid_ar }} %
- Umidade do solo: {{ $json.umid_solo }} %
- Luminosidade: {{ $json.luz }} %

O modelo TinyML embarcado classificou a situação como: {{ $json.classe }}.
Ação que o robô já executou sozinho: {{ $json.acao }}.

Gere a recomendação para o pequeno produtor em JSON.')
        }]
      },
      options: {
        instructions: 'Você é o assistente agronômico do AgroConecta, que apoia pequenos produtores rurais e agricultores familiares. Responda SOMENTE com um JSON válido no formato {"curta": "...", "completa": "..."}. "curta": até 60 caracteres, em português SEM acentos (vai para um display OLED), no imperativo. "completa": até 280 caracteres, em português, dizendo o que está acontecendo, por que importa e o que fazer nas próximas horas. Seja prático e não invente dados além das leituras.',
        maxTokens: 300,
        temperature: 0.3,
        textFormat: { textOptions: { type: 'json_object' } }
      }
    }
  },
  output: [{ output: [{ content: [{ type: 'output_text', text: { curta: 'Irrigar agora: solo seco', completa: 'Solo com 22% de umidade e 33 °C.' } }] }] }]
});

const recomendacao = node({
  type: 'n8n-nodes-base.set',
  version: 3.4,
  config: {
    name: 'Interpretar resposta da IA',
    position: [960, 180],
    parameters: {
      mode: 'raw',
      jsonOutput: expr('{{ JSON.stringify((() => { const t = $("Ler telemetria").item.json; const v = $json.output?.[0]?.content?.[0]?.text ?? $json.content?.[0]?.text ?? $json.text ?? null; let r = null; if (v && typeof v === "object") { r = v; } else if (typeof v === "string") { try { r = JSON.parse(v.replace(/```json|```/g, "").trim()); } catch (e) { r = null; } } if (r && !r.curta) { r = null; } const local = { IRRIGAR: "Solo seco: irrigar", ESTRESSE_TERMICO: "Calor: sombrear e irrigar", RISCO_FUNGO: "Umidade alta: risco de fungo", RISCO_GEADA: "Frio: proteger a cultura" }; return Object.assign({}, t, { fonte_ia: r ? "gpt-4o-mini" : "fallback_local", recomendacao_curta: String(r?.curta ?? local[t.classe] ?? "Verificar a lavoura").slice(0, 60), recomendacao_completa: r?.completa ?? ("Alerta " + t.classe + " detectado pelo robo. IA na nuvem indisponivel; seguindo a acao local: " + t.acao) }); })()) }}')
    }
  },
  output: [{ robo_id: 'robo-campo-01', classe: 'IRRIGAR', recomendacao_curta: 'Irrigar agora: solo seco', recomendacao_completa: 'Solo com 22%...', fonte_ia: 'gpt-4o-mini' }]
});

const semIA = node({
  type: 'n8n-nodes-base.set',
  version: 3.4,
  config: {
    name: 'Sem chamada à IA',
    position: [720, 440],
    parameters: {
      mode: 'manual',
      includeOtherFields: true,
      assignments: {
        assignments: [
          { id: 's1', name: 'recomendacao_curta', value: expr('{{ $json.sensor_ok ? ($json.classe === "NORMAL" ? "Lavoura OK" : "Situacao mantida") : "Verificar sensor" }}'), type: 'string' },
          { id: 's2', name: 'recomendacao_completa', value: '', type: 'string' },
          { id: 's3', name: 'fonte_ia', value: 'nenhuma', type: 'string' }
        ]
      }
    }
  },
  output: [{ robo_id: 'robo-campo-01', classe: 'NORMAL', recomendacao_curta: 'Lavoura OK', recomendacao_completa: '', fonte_ia: 'nenhuma' }]
});

const gravar = node({
  type: 'n8n-nodes-base.postgres',
  version: 2.6,
  config: {
    name: 'Gravar leitura no Supabase',
    position: [1200, 300],
    credentials: PG,
    parameters: {
      operation: 'executeQuery',
      query: `INSERT INTO leituras_robo (robo_id, temp_ar, umid_ar, umid_solo, luz, distancia_cm, classe, acao, modo,
                           sensor_ok, irrigando, evento, recomendacao_ia, payload)
SELECT r.robo_id, r.temp_ar, r.umid_ar, r.umid_solo, r.luz, r.distancia_cm, r.classe, r.acao, r.modo,
       r.sensor_ok, r.irrigando, r.evento, NULLIF(r.recomendacao_completa, ''), $1::jsonb
FROM jsonb_to_record($1::jsonb) AS r(robo_id text, temp_ar numeric, umid_ar numeric, umid_solo numeric, luz numeric,
     distancia_cm numeric, classe text, acao text, modo text, sensor_ok boolean, irrigando boolean, evento boolean,
     recomendacao_completa text)
RETURNING id;`,
      options: { queryReplacement: expr('{{ [JSON.stringify($json)] }}') }
    }
  },
  output: [{ id: 1 }]
});

const responder = node({
  type: 'n8n-nodes-base.respondToWebhook',
  version: 1.4,
  config: {
    name: 'Responder ao robô',
    position: [1440, 300],
    parameters: {
      respondWith: 'json',
      responseBody: expr('{{ JSON.stringify({ ok: true, id: $json.id, recomendacao: ($("Interpretar resposta da IA").isExecuted ? $("Interpretar resposta da IA").first().json.recomendacao_curta : $("Sem chamada à IA").first().json.recomendacao_curta), fonte: ($("Interpretar resposta da IA").isExecuted ? $("Interpretar resposta da IA").first().json.fonte_ia : "nenhuma") }) }}'),
      options: {}
    }
  },
  output: [{}]
});

const assinantes = node({
  type: 'n8n-nodes-base.postgres',
  version: 2.6,
  config: {
    name: 'Buscar produtores inscritos',
    position: [1200, 60],
    credentials: PG,
    executeOnce: true,
    parameters: { operation: 'executeQuery', query: 'SELECT chat_id FROM assinantes_telegram WHERE ativo;' }
  },
  output: [{ chat_id: 123456789 }]
});

const avisar = node({
  type: 'n8n-nodes-base.telegram',
  version: 1.2,
  config: {
    name: 'Avisar produtor no Telegram',
    position: [1440, 60],
    credentials: TG,
    onError: 'continueRegularOutput',
    parameters: {
      resource: 'message',
      operation: 'sendMessage',
      chatId: expr('{{ $json.chat_id }}'),
      text: expr('🚜 <b>Robô de campo — {{ $("Interpretar resposta da IA").first().json.classe.replace("_", " ") }}</b>\n🌡 {{ $("Interpretar resposta da IA").first().json.temp_ar }} °C · 💧 ar {{ $("Interpretar resposta da IA").first().json.umid_ar }}% · 🌱 solo {{ $("Interpretar resposta da IA").first().json.umid_solo }}% · ☀️ luz {{ $("Interpretar resposta da IA").first().json.luz }}%\n🤖 Ação do robô: {{ $("Interpretar resposta da IA").first().json.acao }}\n\n{{ String($("Interpretar resposta da IA").first().json.recomendacao_completa).replace(/&/g, "&amp;").replace(/</g, "&lt;") }}'),
      additionalFields: { appendAttribution: false, parse_mode: 'HTML' }
    }
  },
  output: [{ ok: true }]
});

const nota = sticky(`## 🚜 Robô de Campo AgroConecta (Maker Lab)
O ESP32 envia a telemetria. Quando o modelo TinyML embarcado detecta uma situação nova, um LLM (GPT-4o mini) gera a recomendação: a versão curta volta para o display do robô e a completa vai ao produtor no Telegram. Tudo fica registrado no Supabase.`, [receber, ler, precisaIA], { color: 4 });

export default workflow('agroconecta-robo-campo', 'AgroConecta | Robô de campo (Maker Lab)')
  .add(receber)
  .to(ler)
  .to(precisaIA
    .onTrue(ia.to(recomendacao))
    .onFalse(semIA.to(gravar)))
  .add(recomendacao)
  .to(gravar)
  .add(recomendacao)
  .to(assinantes)
  .to(avisar)
  .add(gravar)
  .to(responder)
  .add(nota);
