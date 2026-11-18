"""Verifica se o modelo exportado para C++ (modelo_tinyml.h) dá as MESMAS respostas do modelo Python.

Traduz a função predict() do cabeçalho C++ de volta para Python e compara as previsões com as do
scikit-learn em 6.000 amostras novas. Assim garantimos que o que roda no ESP32 é o modelo treinado.

Uso:  python verificar_exportacao.py
"""
from __future__ import annotations

import re
from pathlib import Path

import numpy as np
from sklearn.model_selection import train_test_split
from sklearn.tree import DecisionTreeClassifier

import treinar_modelo as tm

CABECALHO = Path(__file__).resolve().parent.parent / "firmware" / "modelo_tinyml.h"


def traduzir_predict(codigo_cpp: str):
    corpo = codigo_cpp.split("int predict(float *x) {", 1)[1]
    linhas_py, nivel, profundidade = ["def predict(x):"], 1, 1
    for bruta in corpo.splitlines():
        linha = bruta.strip()
        if not linha:
            continue
        if linha.startswith("if ("):
            cond = re.match(r"if \((.+)\) \{", linha).group(1)
            linhas_py.append("    " * nivel + f"if {cond}:")
            nivel += 1; profundidade += 1
        elif linha.startswith("else {"):
            linhas_py.append("    " * nivel + "else:")
            nivel += 1; profundidade += 1
        elif linha.startswith("return "):
            linhas_py.append("    " * nivel + linha.rstrip(";"))
        elif linha == "}":
            nivel -= 1; profundidade -= 1
            if profundidade == 0:
                break
    escopo: dict = {}
    exec("\n".join(linhas_py), escopo)
    return escopo["predict"]


def main():
    predict_cpp = traduzir_predict(CABECALHO.read_text(encoding="utf-8"))

    # mesmo treino do treinar_modelo.py (sementes fixas)
    tm.rng = np.random.default_rng(42)
    x, y = tm.gerar(6000)
    x_tr, _, y_tr, _ = train_test_split(x, y, test_size=0.25, random_state=7, stratify=y)
    modelo = DecisionTreeClassifier(max_depth=7, min_samples_leaf=8, random_state=7).fit(x_tr, y_tr)

    tm.rng = np.random.default_rng(2026)   # amostras NOVAS, nunca vistas
    x_novo, _ = tm.gerar(6000)
    esperado = modelo.predict(x_novo)
    obtido = np.array([predict_cpp(list(map(float, linha))) for linha in x_novo])
    iguais = int((esperado == obtido).sum())
    print(f"Previsões idênticas entre Python e C++ exportado: {iguais}/{len(x_novo)}")
    assert iguais == len(x_novo), "O modelo exportado diverge do modelo treinado!"
    print("OK: o modelo que roda no ESP32 é exatamente o modelo treinado.")


if __name__ == "__main__":
    main()
