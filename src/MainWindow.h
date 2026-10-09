#pragma once

#include "Presupuesto.h"

#include <QMainWindow>
#include <memory>

class QCloseEvent;
class QLabel;
class QLineEdit;
class QTreeView;
class QTableView;
class QStackedWidget;
class QTabWidget;
class GastoRealWidget;
class QStandardItemModel;
class ModeloPresupuesto;
class FiltroPresupuesto;

// Ventana equivalente al visor HTML: cifras, árbol, búsqueda, detalle.
// No genera HTML. El detalle se rellena al seleccionar una partida.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    void cargarRuta(const QString& ruta); // presupuesto .json o proyecto .bc3proj

protected:
    void closeEvent(QCloseEvent* evento) override;

private slots:
    void abrir();
    void buscar(const QString& texto);
    void seleccionCambiada();
    void ponerArbol();
    void ponerLista();
    void expandir();
    void colapsar();

private:
    bool cargarPresupuestoDe(const QString& ruta);
    bool cargarProyecto(const QString& ruta);
    bool escribirProyecto(const QString& ruta);
    void abrirProyecto();
    bool guardarProyecto();
    bool guardarProyectoComo();
    bool confirmarDescarte();
    void marcarModificado(bool modificado);
    void actualizarTitulo();

    void pintarCabecera();
    void rellenarLista();
    void pintarDetalle(const Fila* fila);

    std::unique_ptr<Presupuesto> presupuesto_;
    QString rutaJson_;      // presupuesto abierto (ruta absoluta)
    QString rutaProyecto_;  // .bc3proj actual; vacío si aún no se ha guardado
    bool modificado_ = false;
    ModeloPresupuesto* modelo_ = nullptr;
    FiltroPresupuesto* filtro_ = nullptr;
    QStandardItemModel* lista_ = nullptr;

    QLabel* titulo_ = nullptr;
    QLabel* subtitulo_ = nullptr;
    QLabel* cifras_ = nullptr;
    QLabel* avisos_ = nullptr;
    QLineEdit* buscar_ = nullptr;
    QLabel* estado_ = nullptr;
    QTreeView* arbol_ = nullptr;
    QTableView* tabla_ = nullptr;
    QStackedWidget* pila_ = nullptr;
    QTabWidget* pestanas_ = nullptr;
    GastoRealWidget* gasto_ = nullptr;
    QWidget* detalle_ = nullptr;
    QLabel* detalleTexto_ = nullptr;
    class QTableWidget* tablaDec_ = nullptr;
    class QTableWidget* tablaMed_ = nullptr;
    QLabel* detalleCoste_ = nullptr;
};
