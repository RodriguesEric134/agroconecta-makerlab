// Gerado automaticamente por tinyml/treinar_modelo.py — não editar à mão.
// Entradas (nesta ordem): temp_ar, umid_ar, umid_solo, luz
// Classes: 0=NORMAL, 1=IRRIGAR, 2=ESTRESSE_TERMICO, 3=RISCO_FUNGO, 4=RISCO_GEADA
// Acurácia no conjunto de teste: 0.935
#pragma once
#include <cstdarg>
namespace Eloquent {
    namespace ML {
        namespace Port {
            class DecisionTree {
                public:
                    /**
                    * Predict class for features vector
                    */
                    int predict(float *x) {
                        if (x[2] <= 28.941996574401855) {
                            if (x[2] <= 27.143731117248535) {
                                if (x[0] <= 37.97540855407715) {
                                    if (x[2] <= 24.541690826416016) {
                                        if (x[3] <= 98.63169479370117) {
                                            return 1;
                                        }

                                        else {
                                            if (x[1] <= 38.28099250793457) {
                                                return 1;
                                            }

                                            else {
                                                return 1;
                                            }
                                        }
                                    }

                                    else {
                                        if (x[2] <= 25.094487190246582) {
                                            if (x[1] <= 43.95865249633789) {
                                                return 1;
                                            }

                                            else {
                                                return 1;
                                            }
                                        }

                                        else {
                                            if (x[1] <= 30.773720741271973) {
                                                return 1;
                                            }

                                            else {
                                                if (x[0] <= 20.70208168029785) {
                                                    return 1;
                                                }

                                                else {
                                                    return 1;
                                                }
                                            }
                                        }
                                    }
                                }

                                else {
                                    return 1;
                                }
                            }

                            else {
                                if (x[3] <= 56.3892936706543) {
                                    if (x[3] <= 43.56886672973633) {
                                        return 1;
                                    }

                                    else {
                                        return 1;
                                    }
                                }

                                else {
                                    if (x[0] <= 33.861948013305664) {
                                        if (x[2] <= 28.250411987304688) {
                                            if (x[3] <= 71.50241470336914) {
                                                return 1;
                                            }

                                            else {
                                                return 1;
                                            }
                                        }

                                        else {
                                            return 0;
                                        }
                                    }

                                    else {
                                        return 1;
                                    }
                                }
                            }
                        }

                        else {
                            if (x[0] <= 32.89520835876465) {
                                if (x[0] <= 3.075317144393921) {
                                    if (x[0] <= 2.71082079410553) {
                                        if (x[0] <= 2.078848123550415) {
                                            return 4;
                                        }

                                        else {
                                            if (x[0] <= 2.2360403537750244) {
                                                return 4;
                                            }

                                            else {
                                                if (x[2] <= 57.27419471740723) {
                                                    return 4;
                                                }

                                                else {
                                                    return 4;
                                                }
                                            }
                                        }
                                    }

                                    else {
                                        if (x[0] <= 2.8860539197921753) {
                                            return 0;
                                        }

                                        else {
                                            return 4;
                                        }
                                    }
                                }

                                else {
                                    if (x[1] <= 85.61907577514648) {
                                        if (x[0] <= 32.23404884338379) {
                                            if (x[2] <= 30.494930267333984) {
                                                if (x[2] <= 29.8687105178833) {
                                                    return 0;
                                                }

                                                else {
                                                    return 1;
                                                }
                                            }

                                            else {
                                                if (x[0] <= 3.4157663583755493) {
                                                    return 0;
                                                }

                                                else {
                                                    return 0;
                                                }
                                            }
                                        }

                                        else {
                                            if (x[1] <= 29.91162109375) {
                                                if (x[3] <= 79.56040954589844) {
                                                    return 2;
                                                }

                                                else {
                                                    return 2;
                                                }
                                            }

                                            else {
                                                if (x[2] <= 35.941715240478516) {
                                                    return 0;
                                                }

                                                else {
                                                    return 0;
                                                }
                                            }
                                        }
                                    }

                                    else {
                                        if (x[2] <= 63.11619567871094) {
                                            if (x[2] <= 57.83637046813965) {
                                                if (x[1] <= 96.19338607788086) {
                                                    return 0;
                                                }

                                                else {
                                                    return 0;
                                                }
                                            }

                                            else {
                                                if (x[3] <= 55.20766830444336) {
                                                    return 0;
                                                }

                                                else {
                                                    return 0;
                                                }
                                            }
                                        }

                                        else {
                                            if (x[0] <= 14.899958610534668) {
                                                if (x[0] <= 14.440874576568604) {
                                                    return 0;
                                                }

                                                else {
                                                    return 0;
                                                }
                                            }

                                            else {
                                                if (x[0] <= 27.76428508758545) {
                                                    return 3;
                                                }

                                                else {
                                                    return 0;
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            else {
                                if (x[0] <= 35.118797302246094) {
                                    if (x[1] <= 28.438387870788574) {
                                        if (x[1] <= 26.665711402893066) {
                                            return 2;
                                        }

                                        else {
                                            return 2;
                                        }
                                    }

                                    else {
                                        if (x[1] <= 30.22856616973877) {
                                            return 0;
                                        }

                                        else {
                                            if (x[0] <= 34.40409851074219) {
                                                if (x[3] <= 68.38371658325195) {
                                                    return 0;
                                                }

                                                else {
                                                    return 0;
                                                }
                                            }

                                            else {
                                                if (x[1] <= 42.02898025512695) {
                                                    return 0;
                                                }

                                                else {
                                                    return 0;
                                                }
                                            }
                                        }
                                    }
                                }

                                else {
                                    if (x[2] <= 31.8289213180542) {
                                        if (x[0] <= 37.83586120605469) {
                                            return 1;
                                        }

                                        else {
                                            return 2;
                                        }
                                    }

                                    else {
                                        if (x[3] <= 54.43938636779785) {
                                            return 2;
                                        }

                                        else {
                                            if (x[0] <= 35.66945838928223) {
                                                if (x[1] <= 33.633270263671875) {
                                                    return 2;
                                                }

                                                else {
                                                    return 2;
                                                }
                                            }

                                            else {
                                                return 2;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    /**
                    * Predict readable class name
                    */
                    const char* predictLabel(float *x) {
                        return idxToLabel(predict(x));
                    }

                    /**
                    * Convert class idx to readable name
                    */
                    const char* idxToLabel(uint8_t classIdx) {
                        switch (classIdx) {
                            case 0:
                            return "NORMAL";
                            case 1:
                            return "IRRIGAR";
                            case 2:
                            return "ESTRESSE_TERMICO";
                            case 3:
                            return "RISCO_FUNGO";
                            case 4:
                            return "RISCO_GEADA";
                            default:
                            return "Houston we have a problem";
                        }
                    }

                protected:
                };
            }
        }
    }