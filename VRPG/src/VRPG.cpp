#include "VRPG.h"
#include "world.h"
#include "blocks.h"
#include "logger.h"

#define USE_SPOT_LIGHT 0

static const char * dir_names[] = {
	"NORTH",
	"SOUTH",
	"WEST",
	"EAST",
	"UP",
	"DOWN",
};

static bool FLY_MODE = false;

// Declare our game instance
VRPG game;

VRPG::VRPG()
    : _scene(NULL), _wireframe(false)
{
	runWorldUnitTests();
}

Material * createMaterialBlocks();


class MeshVisitor : public CellVisitor {
	FloatArray vertices;
	IntArray indexes;
	FILE * log;
	lUInt64 startTime;
public:
	MeshVisitor() : log(NULL) {
		log = fopen("faces.log", "wt");
		startTime = GetCurrentTimeMillis();
	}
	~MeshVisitor() {
		if (log) {
			fprintf(log, "Time elapsed: %lld millis\n", GetCurrentTimeMillis() - startTime);
			fclose(log);
		}
	}
	virtual void newDirection(Position & camPosition) {
		//fprintf(log, "Cam position : %d,%d,%d \t dir=%d\n", camPosition.pos.x, camPosition.pos.y, camPosition.pos.z, camPosition.direction.dir);
	}
	virtual void visit(World * world, Position & camPosition, Vector3d pos, cell_t cell, int visibleFaces) {
		BlockDef * def = BLOCK_DEFS[cell];
		def->createFaces(world, camPosition, pos, visibleFaces, vertices, indexes);
	}

	Mesh* createMesh() {
		unsigned int vertexCount = vertices.length() / VERTEX_COMPONENTS;
		unsigned int indexCount = indexes.length();
		VertexFormat::Element elements[] =
		{
			VertexFormat::Element(VertexFormat::POSITION, 3),
			VertexFormat::Element(VertexFormat::NORMAL, 3),
			VertexFormat::Element(VertexFormat::COLOR, 3),
			VertexFormat::Element(VertexFormat::TEXCOORD0, 2)
		};
		Mesh* mesh = Mesh::createMesh(VertexFormat(elements, 4), vertexCount, false);
		if (mesh == NULL)
		{
			GP_ERROR("Failed to create mesh.");
			return NULL;
		}
		mesh->setVertexData(vertices.ptr(), 0, vertexCount);
		MeshPart* meshPart = mesh->addPart(Mesh::TRIANGLES, Mesh::INDEX32, indexCount, false);
		meshPart->setIndexData(indexes.ptr(), 0, indexCount);
		return mesh;
	}
};


Material * createMaterialBlocks() {
#if USE_SPOT_LIGHT_LIGHT==1
	Material* material = Material::create("res/shaders/textured.vert", "res/shaders/textured.frag", "SPOT_LIGHT_COUNT 1");
#else
	//SPECULAR;
	Material* material = Material::create("res/shaders/textured.vert", "res/shaders/textured.frag", "VERTEX_COLOR;TEXTURE_DISCARD_ALPHA;POINT_LIGHT_COUNT 1;DIRECTIONAL_LIGHT_COUNT 1");
#endif
	if (material == NULL)
	{
		GP_ERROR("Failed to create material for model.");
		return NULL;
	}
	// These parameters are normally set in a .material file but this example sets them programmatically.
	// Bind the uniform "u_worldViewProjectionMatrix" to use the WORLD_VIEW_PROJECTION_MATRIX from the scene's active camera and the node that the model belongs to.
	material->setParameterAutoBinding("u_worldViewProjectionMatrix", "WORLD_VIEW_PROJECTION_MATRIX");
	material->setParameterAutoBinding("u_inverseTransposeWorldViewMatrix", "INVERSE_TRANSPOSE_WORLD_VIEW_MATRIX");
	material->setParameterAutoBinding("u_cameraPosition", "CAMERA_VIEW_POSITION");
	material->setParameterAutoBinding("u_worldViewMatrix", "WORLD_VIEW_MATRIX");
	//u_specularExponent = 50
	material->getParameter("u_specularExponent")->setValue(50.0f);
	// Set the ambient color of the material.
	material->getParameter("u_ambientColor")->setValue(Vector3(0.4f, 0.4f, 0.4f));
	//u_ambientColor = SCENE_AMBIENT_COLOR

	// Bind the light's color and direction to the material.

	// Load the texture from file.
	Texture::Sampler* sampler = material->getParameter("u_diffuseTexture")->setValue(BLOCK_TEXTURE_FILENAME, true);
	sampler->setFilterMode(Texture::NEAREST_MIPMAP_LINEAR, Texture::NEAREST);
	material->getStateBlock()->setCullFace(true);//true
	material->getStateBlock()->setDepthTest(true);
	material->getStateBlock()->setDepthWrite(true);
	material->getStateBlock()->setBlend(true);
	material->getStateBlock()->setBlendSrc(RenderState::BLEND_SRC_ALPHA);
	material->getStateBlock()->setBlendDst(RenderState::BLEND_ONE_MINUS_SRC_ALPHA);
	//material->getStateBlock()->set
	return material;
}

static int cubeIndex = 1;

Node * VRPG::createWorldNode(Mesh * mesh) {


	Model* cubeModel = Model::create(mesh);
	Node * cubeNode = Node::create("world");
	cubeModel->setMaterial(_material);
	cubeNode->setDrawable(cubeModel);
	//material->release();
	//SAFE_RELEASE(material);
	//cubeModel->release();
	SAFE_RELEASE(cubeModel);
	return cubeNode;
}

class TestVisitor : public CellVisitor {
	FILE * f;
public:
	TestVisitor() {
		f = fopen("facelog.log", "wt");
	}
	~TestVisitor() {
		fclose(f);
	}
	virtual void visitFace(World * world, Position & camPosition, Vector3d pos, cell_t cell, Dir face) {
		fprintf(f, "pos %d,%d,%d  \tcell=%d  \tface=%d\n", pos.x, pos.y, pos.z, cell, face);
	}
};

static short TERRAIN_INIT_DATA[] = {
	//                                      V
	10,  10,  10,  10,  30,  30,  30,  30,  30,  30,  30,  30,  10,  10,  10,  10,  10,
	10,  10,  20,  50,  50,  50,  50,  50,  50,  50,  50,  50,  20,  20,  20,  20,  10,
	10,  20,  20,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  20,  20,  10,
	10,  20,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  20,  10,
	10,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  20,  30,
	30,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  30,
	30,  50,  50,  50,  50,  50,  50,  50, 120,  50,  50,  50,  50,  50,  50,  50,  30,
	30,  50,  50,  50,  50,  50,  50, 110, 140, 130,  50,  50,  50,  50,  50,  50,  30,
	30,  50,  50,  50,  50,  50,  50, 140, 150, 140,  50,  50,  50,  50,  50,  50,  30, // <==
	30,  50,  50,  50,  50,  50,  50, 110, 140, 120,  50,  50,  50,  50,  50,  50,  30,
	30,  50,  50,  50,  50,  50,  50,  50, 110,  50,  50,  50,  50,  50,  50,  50,  30,
	30,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  10,
	30,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  10,
	30,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  40,  50,  10,
	30,  20,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  40,  20,  20,  10,
	30,  20,  20,  50,  50,  50,  50,  50,  50,  50,  40,  20,  20,  20,  20,  20,  10,
	30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  10,  10,  10,  10,  10,
	//                                      ^
};

static short TERRAIN_SCALE_DATA[] = {
	//                                      V
	20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,
	20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,
	20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,
	20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  30,  20,  20,  20,
	20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  45,  20,  20,  20,
	20,  20,  20,  20,  20,  20,  20,  20,  80,  20,  20,  20,  40,  50,  40,  20,  20,
	20,  20,  20,  20,  20,  20,  90,  20,  80,  20,  30,  20,  20,  30,  20,  20,  20,
	20,  20,  20,  20,  20,  90,  20,  80,  30,  20,  40,  20,  20,  20,  20,  20,  20,
	20,  20,  20,  20,  20,  20,  90,  30,  40,  30,  50,  20,  20,  20,  20,  20,  20, // <==
	20,  20,  20,  20,  20,  20,  50,  20,  30,  20,  20,  20,  20,  20,  20,  20,  20,
	20,  20,  20,  20,  20,  20,  40,  70,  40,  90,  20,  40,  20,  20,  20,  20,  20,
	20,  20,  20,  20,  20,  20,  20,  20,  80,  20,  50,  70,  50,  20,  20,  20,  20,
	20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  60,  20,  20,  20,  20,  20,
	20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,
	20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,
	20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,
	20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,
	//                                      ^
};

void VRPG::initWorld() {

	World * world = new World();
	int y0 = 3;

#if 1
	lUInt64 start = GetCurrentTimeMillis();
	CRLog::trace("Generating terrain");

	int terrSizeBits = 10;
	int terrSize = 1 << terrSizeBits;
	TerrainGen scaleterr(terrSizeBits, terrSizeBits); // 512x512
	scaleterr.generate(4321, TERRAIN_SCALE_DATA, terrSizeBits - 4); // init grid is 16x16 (1 << (9-7))
	scaleterr.filter(1);
	//scaleterr.filter(2);
	scaleterr.limit(0, 90);
	TerrainGen terr(terrSizeBits, terrSizeBits); // 512x512
	terr.generateWithScale(123456, TERRAIN_INIT_DATA, terrSizeBits - 4, scaleterr); // init grid is 16x16 (1 << (9-7))
	terr.filter(1);
	terr.limit(5, CHUNK_DY * 3 / 4);
	terr.filter(1);
	for (int x = 0; x < terrSize; x++) {
		for (int z = 0; z < terrSize; z++) {
			int h = terr.get(x, z);
			cell_t cell = 100;
			if (h < CHUNK_DY / 10)
				cell = 100;
			else if (h < CHUNK_DY / 5)
				cell = 101;
			else if (h < CHUNK_DY / 4)
				cell = 102;
			else if (h < CHUNK_DY / 3)
				cell = 103;
			else if (h < CHUNK_DY / 2)
				cell = 104;
			else
				cell = 105;
			for (int y = 0; y < h; y++) {
				world->setCell(x - terrSize / 2, y, z - terrSize / 2, cell);
			}
		}
	}
	y0 = terr.get(terrSize / 2, terrSize / 2) + 8;
	CRLog::trace("terrain generation took %lld ms", GetCurrentTimeMillis() - start);

#endif

	world->getCamPosition().pos = Vector3d(0, y0, 0);
	world->getCamPosition().direction.set(NORTH);

	world->setCell(-5, 3, 5, 1);
	world->setCell(-2, 1, -7, 8);
	world->setCell(-2, 2, -7, 8);
	world->setCell(-2, 3, -7, 8);
	world->setCell(-20, 7, 4, 8);
	world->setCell(20, 6, 9, 8);
	world->setCell(5, 7, -15, 8);
	world->setCell(5, 7, 15, 8);
#if 0
	world->setCell(0, 7, 0, 1);
	world->setCell(5, 7, 0, 1);
	world->setCell(5, 7, 5, 1);
	world->setCell(-5, 7, -5, 1);
	world->setCell(-5, 7, 5, 1);
#else
	for (int x = -100; x <= 100; x++) {
		for (int z = -100; z <= 100; z++) {
			world->setCell(x, 0, z, 3);
		}
	}
	for (int x = -10; x <= 10; x++) {
		for (int z = -10; z <= 10; z++) {
			if (z < -2 || z > 2 || x < -2 || x > 2) {
				world->setCell(x, 8, z, 1);
			}
		}
	}
	for (int x = -10; x <= 10; x++) {
		world->setCell(x, 1, -10, 2);
		world->setCell(x, 2, -10, 2);
		world->setCell(x, 3, -10, 2);
		world->setCell(x, 1, 10, 2);
		world->setCell(x, 2, 10, 2);
		world->setCell(x, 3, 10, 2);
		world->setCell(11, 1, x, 2);
		world->setCell(11, 2, x, 2);
		world->setCell(11, 3, x, 2);
		world->setCell(11, 4, x, 2);
		world->setCell(-11, 1, x, 2);
		world->setCell(-11, 2, x, 2);
		world->setCell(-11, 3, x, 2);
		world->setCell(-11, 4, x, 2);
	}
	for (int i = 0; i < 10; i++) {
		world->setCell(4, 1 + i, 5 + i, 3);
		world->setCell(5, 1 + i, 5 + i, 3);
		world->setCell(6, 1 + i, 5 + i, 3);
		world->setCell(4, 1 + i, 6 + i, 3);
		world->setCell(5, 1 + i, 6 + i, 3);
		world->setCell(6, 1 + i, 6 + i, 3);
	}
	for (int i = 0; i < 5; i++) {
		world->setCell(-6, 1, -6 + i * 2, 50);
		world->setCell(-6, 2, -6 + i * 2, 50);
		world->setCell(-6, 7, -6 + i * 2, 50);
		world->setCell(-6 + i * 2, 1, -6, 50);
		world->setCell(-6 + i * 2, 7, -6, 50);
	}
#endif
	world->setCell(3, 0, -6, 0); // hole
	world->setCell(3, 0, -7, 0); // hole
	world->setCell(4, 0, -6, 0); // hole
	world->setCell(4, 0, -7, 0); // hole

	world->setCell(2, 2, 0, 8);

	_world = world;
}

void VRPG::drawFrameRate(Font* font, const Vector4& color, unsigned int x, unsigned int y, unsigned int fps)
{
	char buffer[128];
	sprintf(buffer, "%s  x:%d z:%d [%d,%d]  h:%d  (F)ly:%s (G)rid:%s  %ufps", 
		dir_names[_world->getCamPosition().direction.dir],
		_world->getCamPosition().pos.x,
		_world->getCamPosition().pos.z,
		_world->getCamPosition().pos.x / 8,
		_world->getCamPosition().pos.z / 8,
		_world->getCamPosition().pos.y,
		HIGHLIGHT_GRID ? "on" : "off",
		FLY_MODE ? "on" : "off",
		fps);
	font->start();
	font->drawText(buffer, x, y, color, font->getSize() * 2);
	font->finish();
}

void VRPG::initialize()
{
	CRLog::setFileLogger("vrpg.log", true);
	CRLog::setLogLevel(CRLog::LL_TRACE);
	CRLog::info("VRPG::initialize()");

	//_font = Font::create("res/arial-distance.gpb");
	_font = Font::create("res/arial.gpb");

	_material = createMaterialBlocks();

	CRLog::trace("initBlockTypes()");
	initBlockTypes();

	CRLog::trace("initWorld()");
	initWorld();

	// Create a new empty scene.
	_scene = Scene::create();

	// Create the camera.
	//Matrix cameraMatrix;
	//Matrix::createPerspective(45.0f, getAspectRatio(), 0.2f, MAX_VIEW_DISTANCE + 1, &cameraMatrix);
	//Matrix cameraShift;
	//Matrix::createTranslation(0, 0.4f, 0, &cameraShift);
	//cameraMatrix.multiply(cameraShift);
	_camera = Camera::createPerspective(60.0f, getAspectRatio(), 0.2f, MAX_VIEW_DISTANCE + 1);
	//camera->setProjectionMatrix(cameraMatrix);
	Node* cameraNode = _scene->addNode("camera");
	_cameraNode = cameraNode;

	// Attach the camera to a node. This determines the position of the camera.
	cameraNode->setCamera(_camera);

	// Make this the active camera of the scene.
	_scene->setActiveCamera(_camera);
	_camera->release();

	// Move the camera to look at the origin.
	//cameraNode->translate(0, 5, 0);
	//cameraNode->rotateY(MATH_DEG_TO_RAD(45.25f));

	// Create a white light.
	Light* dirLight = Light::createDirectional(0.75f, 0.75f, 0.75f);
	Node* dirlightNode = _scene->addNode("dirlight");
	dirlightNode->setLight(dirLight);
	_dirlight = dirLight;
	_dirlightNode = dirlightNode;
	SAFE_RELEASE(dirLight);
	_dirlightNode->rotateX(MATH_DEG_TO_RAD(45));
	_dirlightNode->rotateY(MATH_DEG_TO_RAD(30));

#if	USE_SPOT_LIGHT==1
	Light* light = Light::createSpot(1.5f, 0.75f, 0.75f, 5.0f, MATH_DEG_TO_RAD(60.0f), MATH_DEG_TO_RAD(90.0f));
#else
	Light* light = Light::createPoint(0.8f, 0.8f, 0.8f, 50.0f);
#endif
	Node* lightNode = _scene->addNode("light");
	lightNode->setLight(light);
	//lightNode->translate(0, 5, 0);
	//lightNode->rotateX(MATH_DEG_TO_RAD(-5.25f));
	//lightNode->rotateY(MATH_DEG_TO_RAD(45.25f));
	_light = light;
	_lightNode = lightNode;
	// Release the light because the node now holds a reference to it.
	SAFE_RELEASE(light);
	//lightNode->rotateX(MATH_DEG_TO_RAD(-25.0f));


#if	USE_SPOT_LIGHT==1
	material->getParameter("u_spotLightColor[0]")->setValue(_lightNode->getLight()->getColor());
	material->getParameter("u_spotLightInnerAngleCos[0]")->setValue(_lightNode->getLight()->getInnerAngleCos());
	material->getParameter("u_spotLightOuterAngleCos[0]")->setValue(_lightNode->getLight()->getOuterAngleCos());
	material->getParameter("u_spotLightRangeInverse[0]")->setValue(_lightNode->getLight()->getRangeInverse());
	material->getParameter("u_spotLightDirection[0]")->bindValue(_lightNode, &Node::getForwardVectorView);
	material->getParameter("u_spotLightPosition[0]")->bindValue(_lightNode, &Node::getTranslationView);
#else
	_material->getParameter("u_pointLightColor[0]")->setValue(_lightNode->getLight()->getColor());
	_material->getParameter("u_pointLightPosition[0]")->bindValue(_lightNode, &Node::getForwardVectorWorld);
	_material->getParameter("u_pointLightRangeInverse[0]")->bindValue(_lightNode->getLight(), &Light::getRangeInverse);
#endif
	_material->getParameter("u_directionalLightColor[0]")->setValue(_lightNode->getLight()->getColor());
	//_material->getParameter("u_ambientColor")->setValue(Vector3(0.0f, 0.0f, 0.0f));
	_material->getParameter("u_directionalLightDirection[0]")->bindValue(_dirlightNode, &Node::getForwardVectorView); 

	_group2 = _scene->addNode("group2");
#if 0
	int sz = 50;
	for (int x = -sz; x <= sz; x++) {
		for (int y = -sz; y <= sz; y++) {
			_group2->addChild(createCube(x, -2, y));
		}
		_group2->addChild(createCube(x, -1, -sz));
		_group2->addChild(createCube(x, -1, sz));
		_group2->addChild(createCube(x, 0, -sz));
		_group2->addChild(createCube(x, 0, sz));
		_group2->addChild(createCube(x, 1, -sz));
		_group2->addChild(createCube(x, 1, sz));
	}
#endif



	//TestVisitor * visitor = new TestVisitor();
	////world->visitVisibleCells(world->getCamPosition(), visitor);
	//world->visitVisibleCellsAllDirections(world->getCamPosition(), visitor);
	//
	//delete visitor;

	MeshVisitor * meshVisitor = new MeshVisitor();
	//world->visitVisibleCells(world->getCamPosition(), meshVisitor);
	_world->visitVisibleCellsAllDirectionsFast(_world->getCamPosition(), meshVisitor);
	Mesh * worldMesh = meshVisitor->createMesh();
	_worldMesh = worldMesh;
	_worldNode = createWorldNode(worldMesh);
	SAFE_RELEASE(worldMesh);
	delete meshVisitor;

	_group2->addChild(_worldNode);

}

void VRPG::finalize()
{
    SAFE_RELEASE(_scene);
	SAFE_RELEASE(_material);
	delete _world;
}

static bool animation = false;

void VRPG::update(float elapsedTime)
{
    // Rotate model
    //_scene->findNode("box")
	//_group1->rotateY(MATH_DEG_TO_RAD((float)elapsedTime / 1000.0f * 180.0f));
	//_cubeNode->rotateY(MATH_DEG_TO_RAD((float)elapsedTime / 1000.0f * 180.0f));
	//_cameraNode->rotateY(MATH_DEG_TO_RAD((float)elapsedTime / 15000.0f * 180.0f));
	//_cubeNode2->rotateX(MATH_DEG_TO_RAD((float)elapsedTime / 1000.0f * 180.0f));
	if (animation) {
		//_cameraNode->rotateY(MATH_DEG_TO_RAD((float)elapsedTime / 15000.0f * 180.0f));
		//_lightNode->rotateY(MATH_DEG_TO_RAD((float)elapsedTime / 22356.0f * 180.0f));
		//_lightNode->rotateX(MATH_DEG_TO_RAD((float)elapsedTime / 62356.0f * 180.0f));
		//_group2->rotateY(MATH_DEG_TO_RAD((float)elapsedTime / 5000.0f * 180.0f));
	}
}

void VRPG::render(float elapsedTime)
{
    // Clear the color and depth buffers
    clear(CLEAR_COLOR_DEPTH, Vector4::zero(), 1.0f, 0);

	//setViewport(Rectangle(getWidth() / 8, getHeight() / 5, getWidth() * 6 / 8, getHeight() * 6 / 8));
	setViewport(Rectangle(0, getHeight() / 10, getWidth(), getHeight()));

	_cameraNode->setRotation(Vector3(1, 0, 0), 0);
	_lightNode->setRotation(Vector3(1, 0, 0), 0);

	switch (_world->getCamPosition().direction.dir) {
	case WEST:
		_cameraNode->rotateY(MATH_DEG_TO_RAD(90));
		_lightNode->rotateY(MATH_DEG_TO_RAD(90));
		break;
	case EAST:
		_cameraNode->rotateY(MATH_DEG_TO_RAD(-90));
		_lightNode->rotateY(MATH_DEG_TO_RAD(-90));
		break;
	case NORTH:
		break;
	case SOUTH:
		_cameraNode->rotateY(MATH_DEG_TO_RAD(180));
		_lightNode->rotateY(MATH_DEG_TO_RAD(180));
		break;
    default:
        break;
	}

	Position p = _world->getCamPosition();
	//p.pos -= p.direction.forward;
	_cameraNode->setTranslation(p.pos.x, p.pos.y, p.pos.z);
	_lightNode->setTranslation(p.pos.x, p.pos.y, p.pos.z);
	_cameraNode->translate(0.5, 0.5, 0.5);
	_cameraNode->rotateX(MATH_DEG_TO_RAD(-10));
	//getAspectRatio();
	//getViewport();

	//Matrix m2 = _camera->getProjectionMatrix();
	//m2.m[3] += 0.01;
	//_camera->setProjectionMatrix(m2);


	//_cameraNode->translate(-0.5, -0.5, -0.5);
	//_lightNode->translate(-0.5, -0.5, -0.5);

	//_cameraNode->rotateX(MATH_DEG_TO_RAD(-10));

#define REVISIT_EACH_RENDER 1

	MeshVisitor * meshVisitor = new MeshVisitor();
	_world->visitVisibleCellsAllDirectionsFast(_world->getCamPosition(), meshVisitor);
#if REVISIT_EACH_RENDER==1
	Mesh * oldMesh = _worldMesh;
	Mesh * worldMesh = meshVisitor->createMesh();
	_worldMesh = worldMesh;
	//delete oldMesh;
#endif

	Node * worldNode = createWorldNode(_worldMesh);

#if REVISIT_EACH_RENDER==1
	SAFE_RELEASE(worldMesh);
#endif
	delete meshVisitor;

	_group2->removeAllChildren();

	_group2->addChild(worldNode);

	SAFE_RELEASE(worldNode);

    // Visit all the nodes in the scene for drawing
    _scene->visit(this, &VRPG::drawScene);

	setViewport(Rectangle(0, 0, getWidth(), getHeight() / 10));


	drawFrameRate(_font, Vector4(0, 0.5f, 1, 1), 5, 5, getFrameRate());
}

bool VRPG::drawScene(Node* node)
{
    // If the node visited contains a drawable object, draw it
    Drawable* drawable = node->getDrawable(); 
    if (drawable)
        drawable->draw(_wireframe);

    return true;
}

void VRPG::correctY() {
	Vector3d & pos = _world->getCamPosition().pos;
	if (_world->canPass(pos - Vector3d(2, 3, 2), Vector3d(5, 4, 5))) {
		// down
		while (_world->canPass(pos - Vector3d(2, 4, 2), Vector3d(5, 4, 5)))
			pos.y--;
	} else {
		// up
		while (!_world->canPass(pos - Vector3d(2, 3, 2), Vector3d(5, 4, 5)))
			pos.y++;
	}
}

void VRPG::keyEvent(Keyboard::KeyEvent evt, int key)
{
    if (evt == Keyboard::KEY_PRESS)
    {
		Position * pos = &_world->getCamPosition();
        switch (key)
        {
        case Keyboard::KEY_ESCAPE:
            exit();
            break;
		case Keyboard::KEY_W:
			pos->pos += pos->direction.forward;
			break;
		case Keyboard::KEY_S:
			pos->pos -= pos->direction.forward;
			break;
		case Keyboard::KEY_A:
			pos->pos += pos->direction.left;
			break;
		case Keyboard::KEY_D:
			pos->pos += pos->direction.right;
			break;
		case Keyboard::KEY_Q:
			pos->direction.turnLeft();
			break;
		case Keyboard::KEY_E:
			pos->direction.turnRight();
			break;
		case Keyboard::KEY_Z:
			pos->pos += pos->direction.down;
			break;
		case Keyboard::KEY_C:
			pos->pos += pos->direction.up;
			break;
		case Keyboard::KEY_G:
			HIGHLIGHT_GRID = !HIGHLIGHT_GRID;
			break;
		case Keyboard::KEY_F:
			FLY_MODE = !FLY_MODE;
			break;
		}
		if (!FLY_MODE)
			correctY();
		CRLog::trace("Position: %d,%d,%d direction: %s", pos->pos.x, pos->pos.y, pos->pos.z, dir_names[pos->direction.dir]);
		//Matrix m1 = _camera->getViewMatrix();
		//Matrix m2 = _camera->getProjectionMatrix();
		//CRLog::trace("Aspect ratio: %f", getAspectRatio());
		//CRLog::trace("view matrix:\n%f\t%f\t%f\t%f\n%f\t%f\t%f\t%f\n%f\t%f\t%f\t%f\n%f\t%f\t%f\t%f",
		//	m1.m[0], m1.m[1], m1.m[2], m1.m[3],
		//	m1.m[4], m1.m[5], m1.m[6], m1.m[7],
		//	m1.m[8], m1.m[9], m1.m[10], m1.m[11],
		//	m1.m[12], m1.m[13], m1.m[14], m1.m[15]
		//	);
		//CRLog::trace("projection matrix:\n%f\t%f\t%f\t%f\n%f\t%f\t%f\t%f\n%f\t%f\t%f\t%f\n%f\t%f\t%f\t%f",
		//	m2.m[0], m2.m[1], m2.m[2], m2.m[3],
		//	m2.m[4], m2.m[5], m2.m[6], m2.m[7],
		//	m2.m[8], m2.m[9], m2.m[10], m2.m[11],
		//	m2.m[12], m2.m[13], m2.m[14], m2.m[15]
		//	);
	}
}

void VRPG::touchEvent(Touch::TouchEvent evt, int x, int y, unsigned int contactIndex)
{
    switch (evt)
    {
    case Touch::TOUCH_PRESS:
        _wireframe = !_wireframe;
		if (!_wireframe)
			animation = !animation;
        break;
    case Touch::TOUCH_RELEASE:
        break;
    case Touch::TOUCH_MOVE:
        break;
    };
}
