window.route = (event) => {
    if (!event)
    {
        console.error("event parameter is required.");
        return;
    }
        
    event.preventDefault();
    window.history.pushState({}, "", event.target.href)
    updateContent();
};

export const onContentUpdate = (callback) => {
    if (typeof callback === "function") {
        contentUpdateListeners.push(callback);
    } else {
        console.error("Callback must be a function");
    }
};

const updateContent = async() => {
    let uri = window.location.pathname;

    if (uri === "" || uri === "/") 
        uri = defaultURI

    const htmlPath = getHtmlPath(uri);
    const html = await fetch(htmlPath).then((data) => data.text());
    document.getElementById("content").innerHTML = html;

    const jsPath = getJsPath(uri);
    import(jsPath)
        .then(module => {
            module.onPageLoad();
        });

    contentUpdateListeners.forEach(callback => callback(uri));
};

const getHtmlPath = (uri) => {
    if (!URIs.includes(uri))
        uri = "/404"

    return "/pages" + uri +".html";
};
    
const getJsPath = (uri) => {
    if (!URIs.includes(uri))
        uri = "/404"
    
    return "/js/pages" + uri +".js";
};

const contentUpdateListeners = [];
const URIs = ["/404", "/launch", "/flight-data", "/debug"]
const defaultURI = "/launch";

window.onpopstate = updateContent;

updateContent();