#include "Presupuesto.h"

#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QRegularExpression>

#include <cmath>
#include <functional>

namespace {

QString eurEs(double valor) {
    return formatoEuro(valor);
}

bool hayAlmohadilla(const QJsonObject& n) {
    if (n.value("codigo").toString().endsWith('#'))
        return true;
    const QJsonArray hijos = n.value("hijos").toArray();
    for (const QJsonValue& h : hijos) {
        if (hayAlmohadilla(h.toObject()))
            return true;
    }
    return false;
}

bool esCapitulo(const QJsonObject& n, bool almohadilla) {
    if (almohadilla)
        return n.value("codigo").toString().endsWith('#');
    const QJsonArray hijos = n.value("hijos").toArray();
    for (const QJsonValue& h : hijos) {
        if (!h.toObject().value("hijos").toArray().isEmpty())
            return true;
    }
    return false;
}

QVector<LineaMedicion> parsearLineas(const QString& crudo) {
    const QStringList trozos = crudo.split('\\');
    QVector<LineaMedicion> lineas;
    for (int i = 0; i < trozos.size(); i += 6) {
        QString g[6];
        for (int k = 0; k < 6; ++k)
            g[k] = (i + k < trozos.size()) ? trozos.at(i + k).trimmed() : QString();
        if (g[0].isEmpty() && g[1].isEmpty() && g[2].isEmpty() && g[3].isEmpty() && g[4].isEmpty() && g[5].isEmpty())
            continue;
        lineas.push_back({g[0], g[1], g[2], g[3], g[4], g[5]});
    }
    return lineas;
}

void volcarDescompuesto(const QJsonObject& n, int nivel, QVector<NodoDescompuesto>* salida) {
    const QJsonArray hijos = n.value("hijos").toArray();
    for (const QJsonValue& valor : hijos) {
        const QJsonObject h = valor.toObject();
        NodoDescompuesto d;
        d.nivel = nivel;
        d.codigo = h.value("codigo").toString();
        if (d.codigo.endsWith('#'))
            d.codigo.chop(1);
        d.tipo = h.value("tipo").toString();
        d.unidad = h.value("unidad").toString();
        d.resumen = h.value("resumen").toString();
        const double factorDato = h.value("factor").toDouble();
        const bool sinFactor = !h.contains("factor") || h.value("factor").isNull() || factorDato == 0.0;
        d.cantidad = h.value("cantidad").toDouble() * (sinFactor ? 1.0 : factorDato);
        if (h.contains("precio_venta") && !h.value("precio_venta").isNull())
            d.precio = h.value("precio_venta").toDouble();
        if (h.contains("importe_venta") && !h.value("importe_venta").isNull())
            d.importe = h.value("importe_venta").toDouble();
        d.cantidad = QString::number(d.cantidad, 'f', 6).toDouble();
        salida->push_back(d);
        volcarDescompuesto(h, nivel + 1, salida);
    }
}

QString normalizar(const QString& s) {
    return s.toLower().normalized(QString::NormalizationForm_D).remove(QRegularExpression("\\p{M}"));
}

} // namespace

QString formatoEuro(double valor) {
    QLocale es(QLocale::Spanish, QLocale::Spain);
    return es.toString(valor, 'f', 2) + QStringLiteral(" €");
}

QString formatoNumero(double valor, int decimales) {
    QLocale es(QLocale::Spanish, QLocale::Spain);
    return es.toString(valor, 'f', decimales);
}

bool cargarPresupuesto(const QString& rutaJson, Presupuesto* salida, QString* error) {
    QFile archivo(rutaJson);
    if (!archivo.open(QIODevice::ReadOnly)) {
        if (error) *error = QStringLiteral("No se puede abrir %1").arg(rutaJson);
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(archivo.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error) *error = QStringLiteral("JSON no válido: %1").arg(parseError.errorString());
        return false;
    }
    const QJsonObject datos = doc.object();
    const QJsonArray arbol = datos.value("arbol").toArray();
    if (arbol.isEmpty()) {
        if (error) *error = QStringLiteral("El JSON no trae 'arbol'. ¿Es la salida de bc3_extraer.py?");
        return false;
    }

    QVector<QJsonObject> nodosRaiz;
    bool algunaDoble = false;
    for (const QJsonValue& v : arbol) {
        const QJsonObject o = v.toObject();
        if (o.value("codigo").toString().endsWith(QStringLiteral("##")))
            algunaDoble = true;
    }
    for (const QJsonValue& v : arbol) {
        const QJsonObject o = v.toObject();
        if (!algunaDoble || o.value("codigo").toString().endsWith(QStringLiteral("##")))
            nodosRaiz.push_back(o);
    }

    bool almohadilla = false;
    for (const QJsonValue& v : arbol) {
        if (hayAlmohadilla(v.toObject())) {
            almohadilla = true;
            break;
        }
    }

    QHash<QString, QVector<QJsonObject>> medidas;
    for (const QJsonValue& v : datos.value("mediciones").toArray()) {
        const QJsonObject m = v.toObject();
        const QString clave = m.value("padre").toString() + QChar(0x1f) + m.value("codigo").toString();
        medidas[clave].push_back(m);
    }

    Presupuesto p;
    int desajustes = 0;

    std::function<double(const QJsonObject&, const QString&, int, int, const QStringList&)> visitar;
    visitar = [&](const QJsonObject& n, const QString& padreCod, int nivel, int idPadre, const QStringList& ruta) -> double {
        const int fid = p.filas.size();
        QString nombreVisible = n.value("resumen").toString();
        if (nombreVisible.isEmpty()) {
            nombreVisible = n.value("codigo").toString();
            if (nombreVisible.endsWith('#'))
                nombreVisible.chop(1);
        }

        if (esCapitulo(n, almohadilla)) {
            Fila fila;
            fila.id = fid;
            fila.padre = idPadre;
            fila.nivel = nivel;
            fila.capitulo = true;
            fila.codigo = n.value("codigo").toString();
            if (fila.codigo.endsWith('#'))
                fila.codigo.chop(1);
            fila.resumen = n.value("resumen").toString();
            p.filas.push_back(fila);
            if (idPadre >= 0)
                p.filas[idPadre].hijos.push_back(fid);
            else
                p.raices.push_back(fid);

            double total = 0;
            const QJsonArray hijos = n.value("hijos").toArray();
            for (const QJsonValue& h : hijos)
                total += visitar(h.toObject(), n.value("codigo").toString(), nivel + 1, fid, ruta + QStringList{nombreVisible});
            p.filas[fid].total = std::round(total * 100.0) / 100.0;
            return p.filas[fid].total;
        }

        const double factorCrudo = n.value("factor").toDouble(1.0);
        const bool factorAusente = !n.contains("factor") || n.value("factor").isNull() || factorCrudo == 0.0;
        const double factor = factorAusente ? 1.0 : factorCrudo;
        const double cantidad = n.value("cantidad").toDouble() * factor;

        std::optional<double> precio;
        if (n.contains("precio_venta") && !n.value("precio_venta").isNull())
            precio = n.value("precio_venta").toDouble();
        // Python: `if not precio` — None y 0 se sustituyen por la suma del descompuesto.
        if ((!precio || *precio == 0.0) && !n.value("hijos").toArray().isEmpty()) {
            double suma = 0;
            for (const QJsonValue& h : n.value("hijos").toArray())
                suma += h.toObject().value("importe_venta").toDouble();
            precio = std::round(suma * 10000.0) / 10000.0;
        }
        const double importe = std::round(cantidad * precio.value_or(0.0) * 100.0) / 100.0;

        std::optional<double> precioCoste;
        std::optional<double> importeCoste;
        if (n.contains("precio_coste") && !n.value("precio_coste").isNull()) {
            precioCoste = n.value("precio_coste").toDouble();
            importeCoste = std::round(cantidad * *precioCoste * 100.0) / 100.0;
        }

        const QString cod = n.value("codigo").toString();
        QVector<QJsonObject> registros = medidas.value(padreCod + QChar(0x1f) + cod);
        if (registros.isEmpty())
            registros = medidas.value(QString(QChar(0x1f)) + cod);

        QVector<BloqueMedicion> med;
        std::optional<double> medTotal;
        for (const QJsonObject& m : registros) {
            BloqueMedicion b;
            b.posicion = m.value("posicion").toString();
            if (m.contains("medicion") && !m.value("medicion").isNull()) {
                medTotal = medTotal.value_or(0.0) + m.value("medicion").toDouble();
                b.total = m.value("medicion").toDouble();
            }
            b.lineas = parsearLineas(m.value("lineas").toString());
            med.push_back(b);
        }
        bool desajuste = false;
        if (medTotal && std::abs(*medTotal - cantidad) > 0.0005) {
            desajuste = true;
            ++desajustes;
        }

        Fila fila;
        fila.id = fid;
        fila.padre = idPadre;
        fila.nivel = nivel;
        fila.capitulo = false;
        fila.codigo = cod;
        fila.unidad = n.value("unidad").toString();
        fila.resumen = n.value("resumen").toString();
        fila.cantidad = cantidad;
        fila.precio = precio;
        fila.importe = importe;
        fila.precioCoste = precioCoste;
        fila.importeCoste = importeCoste;
        fila.texto = n.value("texto").toString();
        fila.ruta = ruta;
        fila.medicionTotal = medTotal;
        fila.desajuste = desajuste;
        fila.mediciones = med;
        volcarDescompuesto(n, 0, &fila.descompuestos);
        fila.busqueda = normalizar(QStringList{fila.codigo, fila.resumen, fila.texto, fila.ruta.join(' ')}.join(' '));
        p.filas.push_back(fila);
        if (idPadre >= 0)
            p.filas[idPadre].hijos.push_back(fid);
        else
            p.raices.push_back(fid);
        return importe;
    };

    if (nodosRaiz.size() == 1) {
        const QJsonObject& raiz = nodosRaiz.first();
        p.titulo = raiz.value("resumen").toString();
        if (p.titulo.isEmpty()) {
            p.titulo = raiz.value("codigo").toString();
            if (p.titulo.endsWith('#'))
                p.titulo.chop(1);
        }
        for (const QJsonValue& h : raiz.value("hijos").toArray())
            visitar(h.toObject(), raiz.value("codigo").toString(), 0, -1, {});
    } else {
        for (const QJsonObject& r : nodosRaiz)
            visitar(r, QString(), 0, -1, {});
    }

    double total = 0;
    double coste = 0;
    bool hayCoste = false;
    int nPartidas = 0;
    for (const Fila& f : p.filas) {
        if (f.capitulo)
            continue;
        ++nPartidas;
        total += f.importe;
        if (f.importeCoste) {
            hayCoste = true;
            coste += *f.importeCoste;
        }
    }
    p.total = std::round(total * 100.0) / 100.0;
    p.nPartidas = nPartidas;
    p.nCapitulos = p.filas.size() - nPartidas;
    if (hayCoste)
        p.coste = std::round(coste * 100.0) / 100.0;

    double ref = 0;
    bool hayRef = false;
    for (const QJsonObject& r : nodosRaiz) {
        if (r.contains("importe_venta") && !r.value("importe_venta").isNull()) {
            hayRef = true;
            ref += r.value("importe_venta").toDouble();
        }
    }
    if (hayRef && std::abs(ref - p.total) > 0.05) {
        p.avisos << QStringLiteral("La suma de las partidas (%1) no coincide con el importe de la raíz en el JSON (%2). Revisa cantidades y precios del BC3.")
                        .arg(eurEs(p.total), eurEs(ref));
    }
    if (desajustes) {
        p.avisos << QStringLiteral("%1 partidas tienen una medición ~M distinta de la cantidad del ~D. El importe usa la cantidad del ~D.")
                        .arg(desajustes);
    }
    if (nPartidas == 0)
        p.avisos << QStringLiteral("No se ha encontrado ninguna partida en el árbol.");

    const QJsonObject cab = datos.value("cabecera").toObject();
    p.cabecera.programa = cab.value("programa").toString();
    p.cabecera.propiedad = cab.value("propiedad").toString();
    p.cabecera.version = cab.value("version").toString();
    p.cabecera.tipoNombre = cab.value("tipo_nombre").toString();
    p.cabecera.comentario = cab.value("comentario").toString();

    *salida = std::move(p);
    return true;
}
