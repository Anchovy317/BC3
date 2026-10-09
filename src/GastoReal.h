#pragma once

#include "Presupuesto.h"

#include <QDate>
#include <QHash>
#include <QJsonObject>
#include <QSet>
#include <QString>
#include <QVector>
#include <QWidget>

class QLabel;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

struct GastoPartida {
    QString codigo;
    double importe = 0;
    QString nota;
};

struct GastoMes {
    QDate mes;          // siempre día 1
    double importe = 0; // gasto a mano; solo cuenta si el mes no tiene partidas
    QString nota;
    QVector<GastoPartida> partidas; // desglose opcional

    double total() const {
        if (partidas.isEmpty())
            return importe;
        double s = 0;
        for (const GastoPartida& p : partidas)
            s += p.importe;
        return s;
    }
};

// Partida del presupuesto, lo mínimo para elegirla y comparar.
struct PartidaRef {
    QString codigo;
    QString resumen;
    QString busqueda;
    double referencia = 0; // coste si el JSON lo trae; si no, importe
};

// Pestaña "Gasto real": un importe por mes, con acumulado y comparación contra
// el presupuesto. Cada mes se puede desglosar por partidas (opcional).
// No toca disco: MainWindow guarda y abre el proyecto con aJson() / desdeJson().
class GastoRealWidget : public QWidget {
    Q_OBJECT
public:
    explicit GastoRealWidget(QWidget* parent = nullptr);

    // Llamar al abrir un presupuesto. Deja la tabla vacía.
    void setReferencia(const Presupuesto& presupuesto);

    // Gastos para guardar en el proyecto, y la operación inversa.
    QJsonObject aJson() const;
    void desdeJson(const QJsonObject& raiz);

signals:
    void modificado(); // el usuario cambió algo

private slots:
    void anadirMes();
    void anadirPartidas();
    void quitar();
    void celdaEditada(QTreeWidgetItem* item, int columna);
    void dobleClic(QTreeWidgetItem* item, int columna);

private:
    // selMes/selPartida: qué fila dejar seleccionada. -2 = la que hubiera, -1 = ninguna.
    void repintar(int selMes = -2, int selPartida = -2);
    void actualizarResumen();
    bool editable(const QTreeWidgetItem* item, int columna) const;
    QString nombrePartida(const QString& codigo) const;

    double referencia_ = 0;
    QString etiquetaReferencia_;
    QVector<GastoMes> meses_;
    QSet<QDate> expandidos_;

    QVector<PartidaRef> partidas_;
    QHash<QString, int> indicePartida_;

    QLabel* aviso_ = nullptr;
    QLabel* resumen_ = nullptr;
    QTreeWidget* arbol_ = nullptr;
    QPushButton* bAnadir_ = nullptr;
    QPushButton* bPartidas_ = nullptr;
    QPushButton* bQuitar_ = nullptr;
};
