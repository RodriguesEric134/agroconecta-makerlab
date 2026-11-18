// AgroConecta — Robô de Campo
// Suporte de sensores para o chassi do carrinho (impressão 3D, PLA/PETG)
//
//  - Garra vertical para o sensor CAPACITIVO de umidade do solo (v1.2), com trava por parafuso,
//    permitindo ajustar a profundidade de inserção no canteiro.
//  - Abrigo ventilado (tipo "abrigo meteorológico") para o DHT22: aletas inclinadas deixam o ar
//    circular e protegem da radiação solar direta, evitando leituras de temperatura infladas.
//  - Base com furos de fixação no chassi (padrão de 30 mm entre furos, parafuso M3).
//
// Todas as medidas em milímetros e parametrizadas abaixo.

$fn = 48;

// --------------------------------------------------------------- parâmetros
base_comp        = 70;    // comprimento da base que vai no chassi
base_larg        = 40;
base_esp         = 4;
furo_m3          = 3.4;   // folga para parafuso M3
dist_furos       = 30;

solo_larg_pcb    = 23;    // sensor capacitivo v1.2
solo_esp_pcb     = 1.6;
folga            = 0.4;
garra_alt        = 45;
garra_parede     = 3;

dht_larg         = 15.5;  // DHT22 (15.1 x 25 x 7.7 mm) + folga
dht_alt          = 25.5;
dht_prof         = 8.2;
abrigo_parede    = 2;
aletas           = 5;

// --------------------------------------------------------------- base
module base() {
  difference() {
    hull() {
      for (x = [-base_comp/2 + 6, base_comp/2 - 6], y = [-base_larg/2 + 6, base_larg/2 - 6])
        translate([x, y, 0]) cylinder(r = 6, h = base_esp);
    }
    for (x = [-dist_furos/2, dist_furos/2]) translate([x, 0, -1]) cylinder(d = furo_m3, h = base_esp + 2);
  }
}

// --------------------------------------------------------------- garra do sensor de solo
module garra_solo() {
  larg = solo_larg_pcb + 2*folga + 2*garra_parede;
  prof = solo_esp_pcb + 2*folga + 2*garra_parede;
  translate([base_comp/2 - prof/2 - 2, 0, base_esp])
  difference() {
    translate([-prof/2, -larg/2, 0]) cube([prof, larg, garra_alt]);
    // rasgo vertical por onde a placa do sensor desliza
    translate([-(solo_esp_pcb + 2*folga)/2, -(solo_larg_pcb + 2*folga)/2, -1])
      cube([solo_esp_pcb + 2*folga, solo_larg_pcb + 2*folga, garra_alt + 2]);
    // furo da trava (parafuso M3 aperta a placa na altura desejada)
    translate([-prof, 0, garra_alt * 0.6]) rotate([0, 90, 0]) cylinder(d = furo_m3, h = prof * 2);
  }
}

// --------------------------------------------------------------- abrigo ventilado do DHT22
module abrigo_dht() {
  larg = dht_larg + 2*abrigo_parede;
  prof = dht_prof + 2*abrigo_parede;
  alt  = dht_alt + 2*abrigo_parede;
  translate([-base_comp/2 + prof/2 + 4, 0, base_esp + 12])
  difference() {
    translate([-prof/2, -larg/2, 0]) cube([prof, larg, alt]);
    // cavidade do sensor
    translate([-dht_prof/2, -dht_larg/2, abrigo_parede]) cube([dht_prof, dht_larg, dht_alt + abrigo_parede + 1]);
    // aletas inclinadas nas laterais (ventilação sem sol direto)
    for (i = [0 : aletas - 1])
      translate([0, 0, abrigo_parede + 3 + i * (dht_alt - 6) / (aletas - 1)])
        rotate([0, 0, 0]) rotate([30, 0, 0])
          cube([prof + 2, larg + 2, 1.6], center = true);
    // passagem dos fios por baixo
    translate([0, 0, -1]) cylinder(d = 5, h = abrigo_parede + 2);
  }
  // pilar que eleva o abrigo acima do chassi (afasta do calor dos motores)
  translate([-base_comp/2 + prof/2 + 4, 0, base_esp])
    difference() {
      cylinder(d = 10, h = 12);
      translate([0, 0, -1]) cylinder(d = 5, h = 14);
    }
}

// --------------------------------------------------------------- montagem
base();
garra_solo();
abrigo_dht();
