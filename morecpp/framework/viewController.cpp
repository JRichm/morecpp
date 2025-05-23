#include "viewController.h"

#include <iostream>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../shaders/shaderLoader.h"
#include "../traffic/vehicle.h"


ViewController* ViewController::currentInstance = nullptr;

ViewController::ViewController(int width, int height) : simulationModel(nullptr) {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return;
    }

    // setup
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // create window
    window = glfwCreateWindow(width, height, "Traffic Simulator", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return;
    }
    glfwMakeContextCurrent(window);

    // scroll callback
    currentInstance = this;
    glfwSetScrollCallback(window, scrollCallBack);

    // initialize GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return;
    }
    glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, width, height);

    // load shaders
    shaderProgram = ShaderLoader::loadShaders("shaders/default.vert", "shaders/default.frag");

    // setup verts
    setupRectangleVerticies();

    // set up camera
    cameraPos = glm::vec3(0.0f, 100.0f, 0.0f);
    cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
    cameraUp = glm::vec3(0.0f, 0.0f, -1.0f);
}


ViewController::~ViewController() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);

    if (window) {
        glfwDestroyWindow(window);
    }

    if (currentInstance == this) {
        glfwTerminate();
        currentInstance = nullptr;
    }
}


void ViewController::scrollCallBack(GLFWwindow* window, double xOffset, double yOffset) {
	if (currentInstance) {
		currentInstance->processScroll(yOffset);
	}
}


void ViewController::processScroll(double yOffset) {
    float zoomSensitivity = 10.0f;
    float zoomFactor = 1.0f + (-yOffset * zoomSensitivity / 100.0f);

    orthographicLeft *= zoomFactor;
    orthographicRight *= zoomFactor;
    orthographicBottom *= zoomFactor;
    orthographicTop *= zoomFactor;

    // limit zoom
    orthographicLeft = std::max(orthographicLeft, -3200.0f);
    orthographicRight = std::min(orthographicRight, 3200.0f);
    orthographicBottom = std::max(orthographicBottom, -1800.0f);
    orthographicTop = std::min(orthographicTop, 1800.0f);
}


void ViewController::setupRectangleVerticies() {
    float vertices[] = {
        0.5f, 0.0f,  0.5f,  // top right
        0.5f, 0.0f, -0.5f,  // bottom right
       -0.5f, 0.0f,  0.5f,  // top left

        0.5f, 0.0f, -0.5f,  // bottom right
       -0.5f, 0.0f, -0.5f,  // bottom left
       -0.5f, 0.0f,  0.5f   // top left
    };

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}


void ViewController::drawRectangle(const glm::vec3& position, const glm::vec3& scale, const glm::vec3 & color) {
    // tranformation
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, position);
    model = glm::scale(model, scale);

    // set uniforms
    GLuint modelLoc = glGetUniformLocation(shaderProgram, "model");
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    GLuint colorLoc = glGetUniformLocation(shaderProgram, "objectColor");
    glUniform3fv(colorLoc, 1, glm::value_ptr(color));

    // draw rectangle
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}


bool ViewController::processEvents() {
    glfwPollEvents();

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
        return false;
    }

    float baseCameraSpeed = 1.0f;

    float zoomLevel = getCurrentZoomLevel();
    float cameraSpeed = baseCameraSpeed * zoomLevel;


    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        cameraPos.z -= cameraSpeed;
        cameraTarget.z -= cameraSpeed;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        cameraPos.x -= cameraSpeed;
        cameraTarget.x -= cameraSpeed;
    }

    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        cameraPos.z += cameraSpeed;
        cameraTarget.z += cameraSpeed;
    }

    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        cameraPos.x += cameraSpeed;
        cameraTarget.x += cameraSpeed;
    }

    cameraTarget.x = cameraPos.x;
    cameraTarget.z = cameraPos.z;
    cameraTarget.y = 0.0f;

    return true;
}


void ViewController::moveCamera(float deltaX, float deltaY) {
    float zoomLevel = getCurrentZoomLevel();

    cameraPos.x += deltaX * zoomLevel;
    cameraPos.z += deltaY * zoomLevel;
    cameraTarget.x = cameraPos.x;
    cameraTarget.z = cameraPos.z;
    cameraTarget.y = 0.0f;
}


void ViewController::render() {
    if (!simulationModel) {
        std::cerr << "No simulation model set for rendering" << std::endl;
        return;
    }

    // clear the screen
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(shaderProgram);

    glm::mat4 view = glm::lookAt(
        cameraPos,
        cameraTarget,
        cameraUp
    );

    glm::mat4 projection = glm::ortho(
        orthographicLeft, orthographicRight,
        orthographicBottom, orthographicTop,
        0.1f, 200.0f
    );

    GLint viewLoc = glGetUniformLocation(shaderProgram, "view");
    GLint projLoc = glGetUniformLocation(shaderProgram, "projection");
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

    auto roadSegments = simulationModel->getAllRoadSegments();
    for (const auto& road : roadSegments) {
        renderRoadSegment(*road);
    }

    auto junctions = simulationModel->getAllJunctions();
    for (const auto& junction : junctions) {
        renderJunction(*junction);
    }

    glfwSwapBuffers(window);
}



void ViewController::renderJunction(const Junction& junction) {
    const Vector3& junctionPos = junction.getPosition();
    float radius = junction.getRadius();

    glm::vec3 position(junctionPos.x, 0.01f, junctionPos.z);
    glm::vec3 scale(radius * 2, 0.5f, radius * 2);

    glm::vec3 color(0.4f, 0.4f, 0.4f);

    // color for traffic light
    const TrafficLightJunction* trafficJunction = dynamic_cast<const TrafficLightJunction*>(&junction);
    if (trafficJunction) {
        color = glm::vec3(0.5f, 0.5f, 0.6f);
    }

    drawRectangle(position, scale, color);

    // render traffic lights
    if (trafficJunction) {
        renderTrafficLights(*trafficJunction);
    }
}


void ViewController::renderTrafficLights(const TrafficLightJunction& junction) {
    // loop through roads in junction
    for (const auto& road : junction.getConnectedRoads()) {

        // get entry poiint
        Vector3 entryPoint = junction.getEntryPoint(road);

        // get traffic light state
        LightState state = junction.getLightState(road);

        // set color
        glm::vec3 lightColor;
        switch (state) {
        case LightState::GREEN:
            lightColor = glm::vec3(0.0f, 1.0f, 0.0f);
            break;
        case LightState::YELLOW:
            lightColor = glm::vec3(1.0f, 1.0f, 0.0f);
            break;
        case LightState::RED:
            lightColor = glm::vec3(1.0f, 0.0f, 0.0f);
            break;
        }

        // draw the traffic light
        glm::vec3 lightPos(entryPoint.x, 0.5f, entryPoint.z);
        glm::vec3 lightScale(2.0f, 2.0f, 2.0f);
        drawRectangle(lightPos, lightScale, lightColor);
    }
}


void ViewController::renderRoadSegment(const RoadSegment& road) {

    float zoomFactor = (orthographicRight - orthographicLeft) / 240.0f;
    float minLineThickness = 0.2f * zoomFactor;
    float minLineWidth = 0.5f * zoomFactor;

    auto startJunction = road.getStartJunction();
    auto endJunction = road.getEndJunction();

    // return if no start or end junction
    if (!startJunction || !endJunction) {
        return;
    }

    // get start/end positions
    Vector3 startPos = startJunction->getPosition();
    Vector3 endPos = endJunction->getPosition();
    float junctionRadius = startJunction->getRadius();

    // get direction and length
    Vector3 roadDir = endPos - startPos;
    roadDir = roadDir.normalized();
    float fullLength = roadDir.length();

    Vector3 adjustedStartPos = startPos + roadDir * junctionRadius;
    Vector3 adjustedEndPos = endPos - roadDir * junctionRadius;

    Vector3 adjustedRoadDir = adjustedEndPos - adjustedStartPos;
    float adjustedLength = adjustedRoadDir.length();
   
    if (adjustedLength <= 0.001f) {
        return;
    }

    // get center of road
    glm::vec3 roadCenter(
        (adjustedStartPos.x + adjustedEndPos.x) / 2.0f,
        0.01f,
        (adjustedStartPos.z + adjustedEndPos.z) / 2.0f
    );

    // get road width
    float roadWidth = road.getDimensions().z;

    // get orientation
    float angle = atan2(roadDir.z, roadDir.x) * 180.0f / 3.14159f;


    // transformation matrix
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, roadCenter);
    model = glm::rotate(model, glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, glm::vec3(adjustedLength, 1.0f, roadWidth));

    // draw road
    GLuint modelLoc = glGetUniformLocation(shaderProgram, "model");
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    glm::vec3 roadColor(0.3f, 0.3f, 0.3f);
    GLuint colorLoc = glGetUniformLocation(shaderProgram, "objectColor");
    glUniform3fv(colorLoc, 1, glm::value_ptr(roadColor));

    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    // draw lane markings
    const auto& lanes = road.getLanes();
    int laneCount = road.getLaneCount();
    float laneWidth = roadWidth / laneCount;

    for (size_t i = 1; i < laneCount; i++) {
        bool isShoulderBoundary = (lanes[i - 1].getType() != lanes[i].getType());

        float lanePosition = -roadWidth / 2.0f + i * laneWidth;

        glm::vec3 lineColor(1.0f, 1.0f, 1.0f);

        if (isShoulderBoundary) {
            // solid white (shoulder)
            model = glm::mat4(1.0f);
            model = glm::translate(model, roadCenter);
            model = glm::rotate(model, glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::translate(model, glm::vec3(0.0f, 0.05f, lanePosition));
            model = glm::scale(model, glm::vec3(adjustedLength, std::max(0.2f, minLineThickness), std::max(0.2f, minLineWidth)));

            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
            glUniform3fv(colorLoc, 1, glm::value_ptr(lineColor));

            glBindVertexArray(VAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);

        } else {
            // dotted white (regular)
            float dashLength = 3.0f;
            float gapLength = 7.0f;
            float spacing = dashLength + gapLength;

            int dashCount = static_cast<int>(adjustedLength / spacing) + 1;

            // draw lines
            for (int dashIdx = 0; dashIdx < dashCount; dashIdx++) {
                float dashOffset = -adjustedLength / 2 + dashIdx * spacing + dashLength / 2;

                model = glm::mat4(1.0f);
                model = glm::translate(model, roadCenter);
                model = glm::rotate(model, glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f));
                model = glm::translate(model, glm::vec3(dashOffset, 0.05f, lanePosition));
                model = glm::scale(model, glm::vec3(dashLength, std::max(0.1f, minLineThickness), std::max(0.5f, minLineWidth)));

                glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
                glUniform3fv(colorLoc, 1, glm::value_ptr(lineColor));

                glBindVertexArray(VAO);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                glBindVertexArray(0);
            }
        }
    }

    // Draw vehicles
    for (const auto& vehicle : road.getVehicles()) {
        if (vehicle) {
            renderVehicle(*vehicle, road);
        }
    }
}


void ViewController::renderVehicle(const Vehicle& vehicle, const RoadSegment& road) {
    int lane = vehicle.getCurrentLane();
    float distance = vehicle.getDistanceAlongRoad();

    // get vehicle position
    Vector3 vehiclePos = road.getLanePositionAlongRoad(lane, distance);

    const Vector3& vehicleDim = vehicle.getDimensions();
    const Color& vehicleColor = vehicle.getColor();

    // get road direction for orientation
    Vector3 roadDir = road.getDirection();
    float angle = atan2(roadDir.z, roadDir.x) * 180.0f / 3.14159f;

    // transformation matrix
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(vehiclePos.x, 0.02f, vehiclePos.z));
    model = glm::rotate(model, glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, glm::vec3(vehicleDim.x, 1.0f, vehicleDim.z));

    GLuint modelLoc = glGetUniformLocation(shaderProgram, "model");
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    // set vehicle color
    glm::vec3 vehColor(vehicleColor.r / 255.0f, vehicleColor.g / 255.0f, vehicleColor.b / 255.0f);
    GLuint colorLoc = glGetUniformLocation(shaderProgram, "objectColor");
    glUniform3fv(colorLoc, 1, glm::value_ptr(vehColor));

    // draw vehicle
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

float ViewController::getCurrentZoomLevel() const {
    return (orthographicRight - orthographicLeft) / 200.0f;
}