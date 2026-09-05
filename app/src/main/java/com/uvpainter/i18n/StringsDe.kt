package com.uvpainter.i18n

/** Deutsch. */
val stringsDe = Strings(
    tool = ToolStrings(
        undo = "Rückgängig",
        redo = "Wiederholen",
        brush = "Pinsel",
        eraser = "Radierer",
        fill = "Farbeimer",
        picker = "Pipette",
        stabilizer = "Strich stabilisieren",
        shapesAndBounds = "Formen und Grenzen",
        layers = "Ebenen",
        view = "Ansicht",
        penAndPalm = "Stift und Handballen",
        reference = "Referenzbilder",
        frame = "Einpassen",
        projects = "Projekte",
        document = "Dokument",
        fullscreen = "Vollbild",
        showUi = "Oberfläche zeigen",
        railSize = "GRÖ",
        railOpacity = "DECK",
        railStabilizer = "STAB",
    ),
    brush = BrushStrings(
        title = "Pinsel",
        size = "Größe",
        lockSize = "Größe beim Zoomen festhalten",
        lockSizeHelp = "Die Zahl oben sind dann keine Bildschirmpixel mehr, sondern die " +
            "echte Größe auf dem Modell: näher heranzugehen, um ein Detail " +
            "auszuarbeiten, schrumpft den Strich nicht mehr, bis nichts mehr " +
            "von ihm übrig ist – er wird nur auf dem Bildschirm größer.",
        hardness = "Härte",
        opacity = "Deckkraft",
        flow = "Fluss",
        smoothing = "Feine Glättung",
        spacing = "Abstand",
        tipTitle = "Spitze",
        tipFlatten = "Abflachung",
        tipAngle = "Winkel",
        tipFollowsStroke = "Der Winkel folgt dem Strich",
        grain = "Korn",
        grainScale = "Kornskalierung",
        penTitle = "Verhalten des S-Pen",
        pressureToSize = "Druck → Größe",
        pressureToOpacity = "Druck → Deckkraft",
        sensitivity = "Empfindlichkeit",
        sensitivityHelp = "Erhöhe die Empfindlichkeit, wenn du drücken musst, damit voll gemalt wird.",
        pressureCurve = "Druckkurve",
        minSize = "Mindestgröße",
        tiltToSize = "Neigung → Größe",
        projectionTitle = "Projektion",
        restrictToIsland = "Nur die UV-Insel, auf der ich anfange",
        restrictToIslandHelp = "Damit kannst du ohne Sorge dicht am Rand malen: der Strich " +
            "springt nicht auf den Nachbarbereich über, auch wenn beide auf " +
            "dem Bildschirm aneinanderliegen.",
        noPaintHidden = "Verdecktes nicht bemalen",
        noPaintBackfaces = "Rückseiten nicht bemalen",
        alphaLock = "Alpha sperren",
        edgeFade = "Auslaufen an den Rändern",
    ),
    fill = FillStrings(
        title = "Farbeimer",
        closedArea = "Nur die geschlossene Fläche füllen",
        closedAreaHelp = "Der Eimer breitet sich von der berührten Stelle aus und hält an, " +
            "wo die Farbe wechselt, wie in einem Fotoeditor. Damit füllst du " +
            "eine frei Hand gezeichnete Figur, ohne sie vorher abzugrenzen.",
        islandHelp = "Der Eimer füllt die ganze UV-Insel unter dem Finger.",
        tolerance = "Toleranz",
        toleranceHelp = "Wie stark die Farbe abweichen darf und trotzdem als dieselbe " +
            "Fläche zählt. Läuft die Füllung durch eine Lücke in der Kontur " +
            "aus, senke sie; bleibt sie an einer weichen Kante zu früh " +
            "stehen, erhöhe sie.",
        footer = "Der Eimer malt mit Farbe und Deckkraft des Pinsels und achtet auf " +
            "Grenzen, die eine Fläche schließen.",
    ),
    color = ColorStrings(
        title = "Farbe",
        help = "Genau ist die Farbe nur in der Ansicht «Flach». PBR und Matcap laufen " +
            "durch Beleuchtung und Tonemap und verschieben deshalb den Ton.",
        favoriteOn = "★ Favorit",
        favoriteOff = "☆ Favorit",
        favorites = "Favoriten",
        recents = "Zuletzt benutzt",
    ),
    shape = ShapeStrings(
        title = "Formen",
        help = "Setz den Stift dort auf, wo die Figur beginnt, und zieh: du siehst " +
            "sie live, und beim Abheben sitzt sie fest.",
        polygonSides = "Seiten",
        fromCenter = "Aus der Mitte zeichnen",
        boundsTitle = "Malgrenzen",
        boundsHelp = "Für Modelle mit schlecht geschnittenen UVs: zieh von Hand eine " +
            "Grenze, und der Pinsel überschreitet sie nicht mehr. Fängst du " +
            "einen Strich auf einer Seite an, malt er nicht auf der anderen, " +
            "auch wenn der Pinsel dorthin reicht.\n\n" +
            "Die Fläche muss nicht geschlossen sein: auch eine einzelne Linie " +
            "hält den Pinsel auf ihrer ganzen Länge auf.",
        drawBoundary = "Grenze ziehen",
        clearBoundary = "Grenzen löschen",
        respectBounds = "Grenzen beim Malen beachten",
        boundsActive = "Es sind Grenzen aktiv. Der Farbeimer beachtet nur die, die eine " +
            "Fläche schließen; der Pinsel beachtet auch die offenen.",
        boundsNone = "Es ist noch keine Grenze gezogen.",
        boundsThickness = "Die Grenze wird dicker gezogen als der Pinsel, mit dem du danach " +
            "malst; mit hohem Stabilisator kommt sie gerade heraus, ohne Zittern.",
    ),
    input = InputStrings(
        title = "Stift und Handballenerkennung",
        fingersToNavigate = "Finger, um die Ansicht zu bewegen",
        oneFinger = "1 Finger",
        twoFingers = "2 Finger",
        twoFingersHelp = "Mit zwei Fingern bewegt ein aufgelegter Handballen nichts: eine " +
            "ruhende Hand hinterlässt einen Fleck, keine zwei gewollten Kontakte.",
        rejectWide = "Breite Berührungen verwerfen",
        palmThreshold = "Handballen-Schwelle",
        palmThresholdHelp = "Ein Finger hinterlässt 8–12 mm; die Handkante deutlich mehr. " +
            "Senke die Schwelle, wenn der Handballen noch durchkommt.",
        stylusOnly = "Nur-Stift-Modus (ignoriert Berührungen)",
        fingerPaints = "Der Finger malt auch",
        twistRotates = "Ansicht mit zwei Fingern drehen",
        orbitSensitivity = "Empfindlichkeit beim Umkreisen",
        cameraTitle = "Kamera",
        zoomToPinch = "Der Zoom geht zur Mitte der Zoomgeste",
        zoomToPinchHelp = "Wie das Mausrad am Schreibtisch: zieh die Finger über dem Bein " +
            "des Modells auf, und die Kamera fährt direkt dorthin, ohne vorher " +
            "einzupassen. Aus, kommt der Zoom immer durch die Bildmitte.",
        twoFingerPan = "Zwei Finger verschieben, drei umkreisen",
        twoFingerPanHelp = "Andersherum als üblich. Gut, um die Kamera zu heben und zu " +
            "senken, ohne um das Modell zu kreisen: für die Beine erst runter, dann näher.",
        penButtonTitle = "Taste des S-Pen",
        penButtonHelp = "Der Stift hat nur eine Taste, also entscheide, wofür du sie " +
            "willst. Die mit «solange ich sie halte» gehen beim Loslassen " +
            "von selbst zurück.",
        gesturesTitle = "Gesten",
        gesturesHelp = "2 Finger: umkreisen, aufziehen zum Zoomen und Drehen\n" +
            "3 Finger: verschieben\n" +
            "Tippen mit 2 Fingern: rückgängig\n" +
            "Tippen mit 3 Fingern: wiederholen\n" +
            "Tippen mit 4 Fingern: Oberfläche ausblenden",
    ),
    layers = LayerStrings(
        title = "Ebenen",
        background = "Hintergrund",
        numbered = "Ebene %d",
        copySuffix = "Kopie",
        add = "Ebene hinzufügen",
        duplicate = "Duplizieren",
        delete = "Löschen",
        help = "Gemalt wird immer auf der markierten Ebene. Tippe eine an, um sie zu aktivieren.",
        visibility = "Sichtbarkeit",
        moveUp = "Nach oben",
        moveDown = "Nach unten",
        opacity = "Deckkraft",
        alphaShort = "Alpha",
        clipShort = "Maske",
        lockShort = "Sperr.",
        clearShort = "Leeren",
    ),
    view = ViewStrings(
        title = "Ansicht",
        seams = "UV-Nähte (orange)",
        wireframe = "Gitternetz darüber",
        hideBackfaces = "Rückseiten ausblenden",
        markUnpainted = "Unbemalte Stellen markieren",
        unlitShading = "Volumen in der flachen Ansicht",
        unlitShadingHelp = "Verdunkelt alle drei Kanäle gleich stark, gibt also Form, ohne " +
            "den Ton zu verschieben. Bei 0 ist die Farbe genau die der Auswahl.",
        roughness = "Rauheit",
        metallic = "Metallgrad",
        checkerDensity = "Dichte des Schachbretts",
        frameModel = "Modell einpassen",
        showFps = "FPS anzeigen",
        showFpsHelp = "Solange das an ist, zeichnet die App fortlaufend statt nur bei " +
            "Bedarf, damit die Zahl auch im Ruhezustand stimmt. Mit dem " +
            "Schalter geht alles wieder aus.",
    ),
    doc = DocStrings(
        title = "Dokument",
        meshSummary = "%1\$s\n%2\$d Dreiecke · %3\$d Eckpunkte · %4\$d UV-Inseln",
        meshMaterials = " · %d Materialien",
        unnamedModel = "Modell",
        noModel = "Kein Modell geladen",
        uvWarning = "Hinweis: es gibt UVs außerhalb von 0-1. Das kann ein UDIM-Modell " +
            "sein; vorerst wird nur die erste Kachel bemalt.",
        orientationTitle = "Ausrichtung des Modells",
        yUp = "Y oben",
        zUp = "Z oben",
        flip = "Umkehren",
        rotate90 = "90° drehen",
        atlasTitle = "Auflösung des Atlas",
        atlasHelp = "Ein Wechsel der Auflösung setzt die Ebenen zurück.",
        importModel = "Modell importieren (.glb / .obj)",
        importImage = "Bild in die aktive Ebene importieren",
        exportPng = "PNG nach Downloads exportieren",
        toolbarTitle = "Werkzeugleiste",
        toolbarHelp = "Sie lässt sich am Griff links ziehen und durch Aufziehen darauf " +
            "vergrößern oder verkleinern.",
        toolbarReset = "Zurück an ihren Platz",
        history = "Verlauf: %d MB",
        languageTitle = "Sprache",
        languageHelp = "Stellt die ganze Oberfläche sofort um, ohne Neustart der App.",
    ),
    projects = ProjectStrings(
        title = "Projekte",
        help = "Beim Verlassen der App sichert sich die Sitzung von selbst und ist " +
            "beim nächsten Start wieder da. Speichere mit Namen, um mehrere " +
            "Arbeiten nebeneinander zu haben.",
        nameLabel = "Name des Projekts",
        save = "Speichern",
        newProject = "Neues Projekt",
        refresh = "Liste aktualisieren",
        autoSave = "Automatisches Speichern",
        autoSaveOpen = "Von Zeit zu Zeit bringt es «%s» auf Stand und legt denselben " +
            "Prüfpunkt in der Sitzung ab – das ist es, was beim Öffnen zurückkommt.",
        autoSaveNone = "Es schreibt von Zeit zu Zeit einen Prüfpunkt in die Sitzung, und " +
            "der kommt beim Öffnen der App zurück. Speichere mit Namen, dann " +
            "hält es dieses Projekt auf Stand.",
        minutes = "%d Min.",
        lastCheckpoint = "Letzter Prüfpunkt um %s",
        noCheckpoints = "Noch keine Prüfpunkte in dieser Sitzung.",
        empty = "Es sind noch keine Projekte gespeichert.",
        open = "Öffnen",
        deleteProject = "Projekt löschen",
        openSuffix = "%s · geöffnet",
        savedIn = "Sie liegen in %s",
    ),
    reference = ReferenceStrings(
        title = "Referenz",
        move = "Verschieben",
        picker = "Pipette",
        viewOnly = "Nur ansehen",
        close = "Schließen",
        imageDescription = "Referenzbild",
    ),
    labels = LabelStrings(
        viewUnlit = "Flach",
        viewPbr = "PBR",
        viewMatcap = "Matcap",
        viewUvChecker = "UV-Schachbrett",

        blendNormal = "Normal",
        blendMultiply = "Multiplizieren",
        blendScreen = "Negativ multiplizieren",
        blendOverlay = "Ineinanderkopieren",
        blendAdd = "Addieren",
        blendDodge = "Farbig abwedeln",
        blendBurn = "Farbig nachbelichten",
        blendSoftLight = "Weiches Licht",

        tipRound = "Rund",
        tipFlat = "Flach",
        tipSquare = "Eckig",
        tipSpray = "Spray",

        shapeFree = "Frei",
        shapeLine = "Linie",
        shapeRectangle = "Rechteck",
        shapeEllipse = "Ellipse",
        shapePolygon = "Vieleck",

        penNone = "Nichts",
        penEraseHeld = "Radieren, solange ich sie halte",
        penPickHeld = "Pipette, solange ich sie halte",
        penToggleEraser = "Zwischen Pinsel und Radierer wechseln",
        penUndo = "Rückgängig",
        penRedo = "Wiederholen",
        penToggleStabilizer = "Strichstabilisator ein- oder ausschalten",
        penResetView = "Modell einpassen",
        penToggleUi = "Oberfläche aus- oder einblenden",

        presetInk = "Tuschen",
        presetFlat = "Flache Farbe",
        presetSoftShadow = "Weicher Schatten",
        presetAirbrush = "Airbrush",
        presetFineDetail = "Feines Detail",
        presetRuler = "Reißfeder",
        presetCharcoal = "Kohle",
        presetDryTexture = "Trockene Textur",
    ),
    status = StatusStrings(
        palmRejected = "Aufliegende Hand verworfen",
        sessionRestoreFailed = "Die Sitzung ließ sich nicht wiederherstellen: %s",
        sessionRestored = "Sitzung wiederhergestellt",
        documentFailed = "Das Maldokument ließ sich nicht anlegen",
        nameYourProject = "Gib dem Projekt einen Namen",
        projectSaved = "Projekt gespeichert: %s",
        projectOpened = "Projekt geöffnet: %s",
        projectDeleted = "Projekt gelöscht",
        autoSaveFailed = "Automatisches Speichern fehlgeschlagen: %s",
        newProject = "Neues Projekt",
        boundsCleared = "Grenzen gelöscht",
        modelLoaded = "Modell geladen: %d Dreiecke",
        documentResized = "Dokument auf %dpx (Ebenen zurückgesetzt)",
        nothingToExport = "Es gibt nichts zu exportieren",
        savedToDownloads = "In Downloads gespeichert: %s",
        pngFailed = "Das PNG ließ sich nicht speichern",
        imageImported = "Bild in die aktive Ebene importiert",
        imageImportFailed = "Das Bild ließ sich nicht importieren",
        textureFileName = "textur",
        referenceOpenFailed = "Das Referenzbild ließ sich nicht öffnen",
    ),
    errors = ErrorStrings(
        engineNotReady = "Die Engine ist noch nicht bereit",
        fileUnreadable = "Die Datei ließ sich nicht lesen",
        gltfBadJson = "Das JSON der glTF-Datei ist fehlerhaft",
        noTriangleMesh = "Die Datei enthält kein trianguliertes Mesh",
        glbTruncated = "GLB abgeschnitten",
        notGlb = "Das ist keine GLB-Datei",
        gltf2Only = "Es wird nur glTF 2.0 unterstützt",
        glbNoJsonChunk = "Dem GLB fehlt der JSON-Chunk",
        objNoFaces = "Das OBJ enthält keine Flächen",
        fileTooShort = "Datei leer oder zu kurz",
        unknownFormat = "Format nicht erkannt: .%s (erlaubt: .glb, .gltf, .obj)",
        nothingToSave = "Es gibt noch nichts zu speichern",
        layersUnreadable = "Die Ebenen ließen sich nicht lesen",
        writeFailed = "Konnte nicht nach %s schreiben",
        projectWriteFailed = "Fehler beim Schreiben des Projekts (freier Speicher?)",
        projectCloseFailed = "Die Projektdatei ließ sich nicht schließen",
        projectNotFound = "Projekt nicht gefunden",
        projectInvalid = "Die Projektdatei ist ungültig oder unvollständig",
        unknown = "Fehler der Engine: %s",
    ),
)
