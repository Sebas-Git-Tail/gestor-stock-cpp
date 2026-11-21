// Informe de fallas: cuando una app se cierra por una falla (o le pasa algo que no debería), manda un
// informe anónimo al servidor (POST /fallas/api/informe de servidor-web.py), así se puede arreglar
// aunque nadie avise. Lleva la app, la versión, el sistema y los últimos renglones del registro de la
// app (no los datos: ni montos, ni productos); en el registro, la carpeta personal sale como «~», el
// nombre de usuario como «<usuario>» y las direcciones web solo con el sitio.
//
// Qué cuenta como falla:
//   - la app se cerró sin terminar bien (al abrirla de nuevo se nota que la vez anterior no salió):
//     con la señal si fue un SIGSEGV/SIGABRT… (Linux/Android) o la excepción (Windows)
//   - qFatal / qCritical, y los errores de QML (TypeError, ReferenceError, componentes que no cargan)
//   - lo que la app avise con informar()
// El resto de qWarning/qInfo solo queda en el registro (los últimos ~300 renglones), que va con el informe.
//
// En Android, si el sistema cierra la app en segundo plano no cuenta como falla.
// Los informes que no se pudieron mandar (sin conexión) quedan guardados y se mandan la próxima vez.
// Como mucho 8 informes por día y el mismo error una sola vez por versión.
//
// Uso, en main() después de setApplicationName/setApplicationVersion y antes de crear ventanas:
//     soporte::iniciarFallas();
// Variables para pruebas: COMUN_FALLAS_URL (adónde se manda), COMUN_FALLAS_SIN_RED=1 (no manda nada).
#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <functional>

namespace soporte {

void iniciarFallas();

// Una falla que la app detectó (no se cerró, pero no debería haber pasado). contexto: «sincronizar»,
// «qml», «actualizar»...
void informar(const QString &contexto, const QString &detalle);

// Saca de un texto lo que no tiene que viajar: carpeta personal → «~», nombre de usuario, direcciones web
// (queda solo el sitio), correos.
QString limpiar(const QString &texto);

class Fallas : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enviando READ enviando NOTIFY enviandoCambio)

public:
    static Fallas &instancia();

    bool enviando() const { return enviando_; }
    QString carpeta() const { return carpeta_; }
    QStringList registro() const;                 // los últimos renglones (ya limpios)

    // «Enviar informe de fallas» del menú: manda el registro con un comentario opcional.
    // Emite enviadoManual(true/false) al terminar.
    Q_INVOKABLE void enviarManual(const QString &comentario = QString());
    // Manda lo pendiente; alTerminar se llama cuando terminó (o a los «segundos»), mandado o no.
    void enviarPendientes(std::function<void()> alTerminar = {}, int segundos = 20);
    // Para cuando la app no va a poder seguir (por ejemplo, el QML no cargó): informa, espera un poco a que
    // se mande y recién ahí sale con «codigo».
    void informarYSalir(const QString &contexto, const QString &detalle, int codigo);

    static QUrl destino();

signals:
    void enviandoCambio();
    void enviadoManual(bool ok);

private:
    friend void iniciarFallas();
    friend void informar(const QString &, const QString &);
    Fallas() = default;

    void iniciar();
    void revisarCierreAnterior();
    void guardarInforme(const QString &tipo, const QString &contexto, const QString &detalle,
                        const QStringList &registro, bool siempre = false);
    QJsonObject datosEquipo() const;
    void mandarSiguiente(QStringList archivos, int raiz, std::function<void()> alTerminar);
    void programarEnvio();
    void marcarEnCurso(bool enCurso);

    QString carpeta_;
    bool iniciado_ = false;
    bool enviando_ = false;
    bool envioProgramado_ = false;
};

} // namespace soporte
