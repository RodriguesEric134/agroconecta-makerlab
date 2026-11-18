-- AgroConecta — Robô de Campo (Maker Lab): telemetria enviada pelo ESP32
CREATE TABLE IF NOT EXISTS leituras_robo (
  id                 BIGSERIAL PRIMARY KEY,
  robo_id            TEXT        NOT NULL,
  recebido_em        TIMESTAMPTZ NOT NULL DEFAULT now(),
  temp_ar            NUMERIC(5,1),
  umid_ar            NUMERIC(5,1),
  umid_solo          NUMERIC(5,1),
  luz                NUMERIC(5,1),
  distancia_cm       NUMERIC(6,1),
  classe             TEXT,          -- saída do modelo TinyML embarcado
  acao               TEXT,          -- ação que o robô executou sozinho
  modo               TEXT,          -- PATRULHA | PARADO_ALERTA | IRRIGANDO | DESVIANDO | NOITE | FALHA_SENSOR
  sensor_ok          BOOLEAN,
  irrigando          BOOLEAN,
  evento             BOOLEAN,       -- true quando a classe mudou (pede recomendação da IA)
  recomendacao_ia    TEXT,          -- resposta do LLM (Claude) devolvida ao robô
  payload            JSONB
);
CREATE INDEX IF NOT EXISTS ix_leituras_robo_tempo ON leituras_robo (robo_id, recebido_em DESC);

ALTER TABLE leituras_robo ENABLE ROW LEVEL SECURITY;
DROP POLICY IF EXISTS leitura_publica_robo ON leituras_robo;
CREATE POLICY leitura_publica_robo ON leituras_robo FOR SELECT TO anon, authenticated USING (true);
