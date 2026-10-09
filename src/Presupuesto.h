#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <optional>

// Misma lectura que bc3_visor.py: JSON de bc3_extraer.py, no el .bc3 crudo.
// Capítulo = código acabado en '#' (o, si el fichero no usa almohadilla, nodo
// cuyos hijos también tienen hijos). Partida = lo que cuelga del capítulo.
// Lo que cuelga de la partida es descompuesto.

struct LineaMedicion {
    QString tipo;
    QString comentario;
    QString uds;
    QString longitud;
    QString latitud;
    QString altura;
};

struct BloqueMedicion {
    QString posicion;
    std::optional<double> total;
    QVector<LineaMedicion> lineas;
};

struct NodoDescompuesto {
    int nivel = 0;
    QString codigo;
    QString tipo;
    QString unidad;
    QString resumen;
    double cantidad = 0;
    std::optional<double> precio;
    std::optional<double> importe;
};

struct Fila {
    int id = 0;
    int padre = -1;
    int nivel = 0;
    bool capitulo = false;
    QString codigo;
    QString unidad;
    QString resumen;
    QString texto;
    QStringList ruta;
    double total = 0;       // capítulo: suma de partidas hijas
    double cantidad = 0;    // partida
    std::optional<double> precio;
    double importe = 0;
    std::optional<double> precioCoste;
    std::optional<double> importeCoste;
    std::optional<double> medicionTotal;
    bool desajuste = false; // ~M distinto de la cantidad del ~D
    QVector<BloqueMedicion> mediciones;
    QVector<NodoDescompuesto> descompuestos;
    QVector<int> hijos;
    QString busqueda;       // texto ya normalizado para el filtro
};

struct Cabecera {
    QString programa;
    QString propiedad;
    QString version;
    QString tipoNombre;
    QString comentario;
};

struct Presupuesto {
    QString titulo;
    Cabecera cabecera;
    QVector<Fila> filas;
    QVector<int> raices;
    double total = 0;
    std::optional<double> coste;
    int nPartidas = 0;
    int nCapitulos = 0;
    QStringList avisos;
};

bool cargarPresupuesto(const QString& rutaJson, Presupuesto* salida, QString* error);

QString formatoEuro(double valor);
QString formatoNumero(double valor, int decimales = 3);
