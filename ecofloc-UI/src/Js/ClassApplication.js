class Application {
    constructor(name, listePid, categorie){
        this.name = name;
        this.listePid = listePid;
        this.categorie = categorie;
    }

    getName(){
        return this.name;
    }
    getListePid(){
        return this.listePid;
    }
    getCategorie(){
        return this.categorie;
    }
}