
export const toggleClasses = (selector, condition, classesIfTrue, classesIfFalse) => {
    const elements = document.querySelectorAll(selector);

    const listOfClassesIfTrue = classesIfTrue.split(/\s+/).filter(Boolean);
    const listOfClassesIfFalse = classesIfFalse.split(/\s+/).filter(Boolean);

    elements.forEach(element => {
        const isActive = condition(element);
        element.classList.remove(...(isActive ? listOfClassesIfFalse : listOfClassesIfTrue));
        element.classList.add(...(isActive ? listOfClassesIfTrue : listOfClassesIfFalse));
    });
};