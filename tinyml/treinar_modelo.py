"""Treina o modelo TinyML do Robô de Campo AgroConecta e exporta para C++ (ESP32).

Entradas (4 sensores):  temperatura do ar (°C), umidade do ar (%), umidade do solo (%), luminosidade (%)
Saída (5 classes):      NORMAL, IRRIGAR, ESTRESSE_TERMICO, RISCO_FUNGO, RISCO_GEADA

O conjunto de treino é sintético, gerado a partir de faixas agronômicas de referência,
com ruído de sensor e rótulos ambíguos perto das fronteiras — o modelo precisa
generalizar, não decorar as regras. O resultado é uma árvore de decisão pequena
(poucos KB de flash), que roda em microssegundos no ESP32 sem depender de internet.

Uso:  python treinar_modelo.py      (gera ../firmware/modelo_tinyml.h e metricas.txt)
"""
from __future__ import annotations

from pathlib import Path

import numpy as np
from micromlgen import port
from sklearn.metrics import classification_report, confusion_matrix
from sklearn.model_selection import train_test_split
from sklearn.tree import DecisionTreeClassifier

CLASSES = ["NORMAL", "IRRIGAR", "ESTRESSE_TERMICO", "RISCO_FUNGO", "RISCO_GEADA"]
FEATURES = ["temp_ar", "umid_ar", "umid_solo", "luz"]
AQUI = Path(__file__).resolve().parent
SAIDA_H = AQUI.parent / "firmware" / "modelo_tinyml.h"

rng = np.random.default_rng(42)


def rotular(t: float, ua: float, us: float, luz: float) -> int:
    """Regras agronômicas de referência (prioridade de cima para baixo)."""
    if t <= 3:
        return 4                                   # geada: proteger a cultura
    if us < 30:
        return 1                                   # solo seco: irrigar
    if t >= 35 or (t >= 32 and ua < 30):
        return 2                                   # calor extremo / ar muito seco
    if ua >= 85 and 15 <= t <= 28 and us >= 60 and luz < 60:
        return 3                                   # molhamento foliar prolongado: fungos
    return 0


def gerar(n: int) -> tuple[np.ndarray, np.ndarray]:
    # Mistura de cenários para ter exemplos suficientes de todas as classes
    blocos = [
        (n // 3, (15, 30), (40, 80), (35, 80), (20, 100)),   # dias normais
        (n // 6, (18, 38), (25, 70), (5, 32), (30, 100)),    # solo seco
        (n // 6, (30, 42), (10, 45), (30, 70), (60, 100)),   # calor
        (n // 6, (14, 29), (80, 100), (55, 95), (0, 65)),    # úmido e abafado
        (n // 6, (-4, 8), (50, 95), (35, 80), (0, 40)),      # madrugada fria
    ]
    linhas = []
    for qtd, *faixas in blocos:
        linhas.append(np.column_stack([rng.uniform(lo, hi, qtd) for lo, hi in faixas]))
    x_real = np.vstack(linhas)
    y = np.array([rotular(*linha) for linha in x_real])
    # Ruído típico de sensores baratos (DHT22 ±0,5 °C / ±2%, solo capacitivo ±3%, LDR ±5%)
    ruido = rng.normal(0, [0.5, 2.0, 3.0, 5.0], x_real.shape)
    x = np.clip(x_real + ruido, [-10, 0, 0, 0], [60, 100, 100, 100])
    return x.astype(np.float32), y


def main():
    x, y = gerar(6000)
    x_tr, x_te, y_tr, y_te = train_test_split(x, y, test_size=0.25, random_state=7, stratify=y)
    modelo = DecisionTreeClassifier(max_depth=7, min_samples_leaf=8, random_state=7).fit(x_tr, y_tr)

    pred = modelo.predict(x_te)
    relatorio = classification_report(y_te, pred, target_names=CLASSES, digits=3)
    matriz = confusion_matrix(y_te, pred)
    acuracia = (pred == y_te).mean()
    texto = (f"Modelo: DecisionTreeClassifier(max_depth=7) | folhas: {modelo.get_n_leaves()} | "
             f"profundidade: {modelo.get_depth()}\nAmostras: treino={len(y_tr)} teste={len(y_te)}\n"
             f"Acurácia no teste: {acuracia:.3f}\n\n{relatorio}\nMatriz de confusão (linhas = real):\n"
             f"{'':18}" + " ".join(f"{c[:6]:>7}" for c in CLASSES) + "\n" +
             "\n".join(f"{CLASSES[i]:18}" + " ".join(f"{v:7d}" for v in linha) for i, linha in enumerate(matriz)))
    (AQUI / "metricas.txt").write_text(texto, encoding="utf-8")
    print(texto)

    codigo = port(modelo, classmap=dict(enumerate(CLASSES)))
    cabecalho = (
        "// Gerado automaticamente por tinyml/treinar_modelo.py — não editar à mão.\n"
        f"// Entradas (nesta ordem): {', '.join(FEATURES)}\n"
        f"// Classes: {', '.join(f'{i}={c}' for i, c in enumerate(CLASSES))}\n"
        f"// Acurácia no conjunto de teste: {acuracia:.3f}\n"
    )
    SAIDA_H.parent.mkdir(parents=True, exist_ok=True)
    SAIDA_H.write_text(cabecalho + codigo, encoding="utf-8")
    print(f"\nModelo exportado para {SAIDA_H} ({SAIDA_H.stat().st_size} bytes de código-fonte)")


if __name__ == "__main__":
    main()
