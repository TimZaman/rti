

#include "mainwidget.h"

#include <QMouseEvent>

#include <math.h>

using namespace std;

MainWidget::MainWidget(QWidget *parent) :
    QOpenGLWidget(parent),
    geometries(0),
    /*texture0(0),
    texture1(0),
    texture2(0),
    texture3(0),
    texture4(0),
    texture5(0),*/
    angularSpeed(0)
{
    setAttribute(Qt::WA_StaticContents);
    setMouseTracking(true);

    rti_x=0;
    rti_y=0;

    scaleRatio=1; //zoom ratio
}

MainWidget::~MainWidget()
{
    qDebug() << "MainWidget::~MainWidget()";
    // Make sure the context is current when deleting the texture
    // and the buffers.
    makeCurrent();
    /*delete texture0;
    delete texture1;
    delete texture2;
    delete texture3;
    delete texture4;
    delete texture5;*/

    delete geometries;
    doneCurrent();
}

void MainWidget::wheelEvent(QWheelEvent *e){
    if(e->orientation() == Qt::Vertical){
        scaleRatio = 1+(double)(e->delta()) / 1000;
        qDebug() << scaleRatio;
        //e->Quit;
        //qDebug() << e->delta();
        //scaleRatio +=
        projection.ortho(-1.0f/scaleRatio, 1.0f/scaleRatio, -1.0f/scaleRatio, 1.0f/scaleRatio, -1.0f, 1.0f);
        updateGL();
    }



 

}


void MainWidget::mousePressEvent(QMouseEvent *e)
{
    qDebug() << "mousePressEvent()";
    // Save mouse press position
    mousePressPosition = QVector2D(e->localPos());
    angularSpeed=0;
}

void MainWidget::mouseReleaseEvent(QMouseEvent *e)
{
    qDebug() << "mouseReleaseEvent()";
    // Mouse release position - mouse press position
    QVector2D diff = QVector2D(e->localPos()) - mousePressPosition;

    // Rotation axis is perpendicular to the mouse position difference
    // vector
    QVector3D n = QVector3D(diff.y(), diff.x(), 0.0).normalized();

    // Accelerate angular speed relative to the length of the mouse sweep
    qreal acc = diff.length() / 100.0;

    // Calculate new rotation axis as weighted sum
    rotationAxis = (rotationAxis * angularSpeed + n * acc).normalized();

    // Increase angular speed
    angularSpeed += acc;





}


void MainWidget::mouseMoveEvent(QMouseEvent *e){
    //The setFocus should only be done when a key is pressed
    this->setFocus(Qt::OtherFocusReason); 
    
    QVector2D posNow = QVector2D(e->localPos());
    this->rti_x = (posNow[0]/this->width()*2)-1;
    this->rti_y = (posNow[1]/this->height()*2)-1;

    qDebug() << "rti xy=" << rti_x << "," << rti_y;

    update();
    updateGL();
}

void MainWidget::timerEvent(QTimerEvent *)
{
    //qDebug() << "MainWidget::timerEvent()";  
    // Decrease angular speed (friction)
    angularSpeed *= 0.99;

    //qDebug() << "ang=" << angularSpeed;

    // Stop rotation when speed goes below threshold
    if (angularSpeed < 0.01) {
        angularSpeed = 0.0;
    } else {
        // Update rotation
        rotation = QQuaternion::fromAxisAndAngle(rotationAxis, angularSpeed) * rotation;

        // Request an update
        update();
        updateGL();
    }
}

void MainWidget::initializeGL()
{
    qDebug() << "MainWidget::initializeGL()";
    initializeOpenGLFunctions();

    glClearColor(0, 0, 0, 1);

    initShaders();
    initTextures();

    // Enable depth buffer
    //glEnable(GL_DEPTH_TEST);

    // Enable back face culling
    //glEnable(GL_CULL_FACE);

    geometries = new GeometryEngine;

    // Use QBasicTimer because its faster than QTimer
    timer.start(12, this);
}

void MainWidget::initShaders()
{
    qDebug() << "MainWidget::initShaders()";
    // Compile vertex shader
    if (!program.addShaderFromSourceFile(QOpenGLShader::Vertex, ":/vshader.glsl"))
        close();

    // Compile fragment shader
    if (!program.addShaderFromSourceFile(QOpenGLShader::Fragment, ":/fshader.glsl"))
        close();

    // Link shader pipeline
    if (!program.link())
        close();

    // Bind shader pipeline for use
    if (!program.bind())
        close();
}





void MainWidget::initTextures()
{
    qDebug() << "MainWidget::initTextures()";

    rti_scales.push_back(1.03421378);
    rti_scales.push_back(0.953444779);
    rti_scales.push_back(0.378294528);
    rti_scales.push_back(0.626534283);
    rti_scales.push_back(0.7757743);
    rti_scales.push_back(1.04884148);

    rti_bias.push_back(170);
    rti_bias.push_back(181);
    rti_bias.push_back(143);
    rti_bias.push_back(133);
    rti_bias.push_back(117);
    rti_bias.push_back(-5);


    textures.resize(6);
    QStringList imagelist;
    imagelist << "/Users/tzaman/Dropbox/code/rti/doc/test/1_1.jpg" <<
                "/Users/tzaman/Dropbox/code/rti/doc/test/1_2.jpg" <<
                "/Users/tzaman/Dropbox/code/rti/doc/test/1_3.jpg" <<
                "/Users/tzaman/Dropbox/code/rti/doc/test/1_4.jpg" <<
                "/Users/tzaman/Dropbox/code/rti/doc/test/1_5.jpg" <<
                "/Users/tzaman/Dropbox/code/rti/doc/test/1_6.jpg";

    for (int i=0; i<textures.size(); i++){
        GLuint m_texture;
        //QImage image("../cube.png");
        //QImage image(imagelist[i]);
        //image = image.convertToFormat(QImage::Format_RGB32);

        cv::Mat matIm = cv::imread(imagelist[i].toStdString());
        matIm.convertTo(matIm, CV_32FC3, 1.0/255.0);

        matIm = matIm - cv::Scalar::all(rti_bias[i]/255.0);
        matIm = matIm * rti_scales[i];

        glGenTextures(1, &m_texture);
        glBindTexture(GL_TEXTURE_2D, m_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); //GL_LINEAR_MIPMAP_LINEAR?
        //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, matIm.cols, matIm.rows, 0, GL_BGR, GL_UNSIGNED_BYTE, matIm.data);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, matIm.cols, matIm.rows, 0, GL_BGR, GL_FLOAT, matIm.data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glEnable(GL_TEXTURE_2D);  
        textures[i] =m_texture;
    }

}






void MainWidget::resizeGL(int w, int h)
{
    qDebug() << "MainWidget::resizeGL()";
 
    glViewport( 0, 0, w, h );

    // Calculate aspect ratio
    qreal aspect = qreal(w) / qreal(h ? h : 1);

    // Set near plane to 3.0, far plane to 7.0, field of view 45 degrees
    const qreal zNear = 3.0, zFar = 7.0, fov = 45.0;

    // Reset projection
    projection.setToIdentity();

   // float scalefac = 1; //This is the amount of zoom

    //float scalefac = 1.0f/500.0f; 
    //modelMatrix.ortho( -width/scalefac, width/scalefac, -height/scalefac, height/scalefac, -1.0f, 1.0f );

    // Set perspective projection
    //projection.ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
    projection.ortho(-1.0f/scaleRatio, 1.0f/scaleRatio, -1.0f/scaleRatio, 1.0f/scaleRatio, -1.0f, 1.0f);


    //projection.perspective(fov, aspect, zNear, zFar);


}

/*
GLfloat * qvec2glfloat(QVector<float> qfl){
    GLfloat glfl[qfl.size()];
    for (int i=0;i<qfl.size()){
        qlfl
    }
}*/


void MainWidget::paintGL()
{
    qDebug() << "MainWidget::paintGL()";
    // Clear color and depth buffer
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    //texture0->bind();
   // texture1->bind();

    // Calculate model view transformation
    QMatrix4x4 matrix;
    matrix.translate(0.0, 0.0, 0.0);
   // matrix.rotate(rotation);

    // Set modelview-projection matrix
    program.setUniformValue("mvp_matrix", projection * matrix);


   // GLfloat biases[] = {}
   // GLfloat scales[] = {}


    program.setUniformValueArray("scales", reinterpret_cast<GLint*>(&rti_bias[0]), rti_bias.size());
    program.setUniformValueArray("biases", reinterpret_cast<GLfloat*>(&rti_scales[0]), rti_scales.size(), 1);
    
    GLfloat weights[] = {rti_x*rti_x, rti_y*rti_y, rti_x*rti_y, rti_x, rti_y, 1};
    program.setUniformValueArray("weights", weights, 6, 1);



   // #define GL_TEXTURE2D 0x0DE1

    //const QString& filename; 
 





    //glActiveTexture(GL_TEXTURE0);
    glEnable(GL_TEXTURE_2D);
   // glBindTexture(GL_TEXTURE_2D, m_texture);

    //int i=0;
    //glActiveTexture(GL_TEXTURE0 + i);
    for (int i=0; i<textures.size(); i++){
        glActiveTexture(GL_TEXTURE0+i);
        glBindTexture(GL_TEXTURE_2D, textures[i]);
    }



    for (int i=0; i<textures.size(); i++){
        int n_Texture = program.uniformLocation("texture["+QString::number(i)+"]");
        program.setUniformValue(n_Texture,i);
    }


    //program.setUniformValue("texture", m_texture); 


    //program.bind();








    // Use texture unit 0 which contains cube.png
   // program.setUniformValue("texture[5]", 0);
    //program.setUniformValue("textures", texture1);

    // Draw cube geometry
    geometries->drawCubeGeometry(&program);
}









